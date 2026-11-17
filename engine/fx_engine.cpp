/*
 * fx_engine.cpp - FxSound live audio engine for Linux (PipeWire).
 *
 * Graph:   apps --> [FxSound]  (Audio/Sink, capture stream)
 *                       |  FxSound DSP (same code as Windows, bit-identical)
 *                       v
 *                   [FxSound playback stream] --> real output device
 *
 * The two streams share node.group (same driver/clock, no drift) and node.link-group (so the
 * session manager never links the playback stream back into FxSound itself). The capture stream
 * triggers the playback stream, which does the processing - the same scheme as PipeWire's own
 * module-filter-chain - so the added latency is zero quanta.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "fx_engine.h"
#include "DfxDsp.h"

#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/json.h>
#include <spa/utils/result.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <codecvt>
#include <cstdarg>
#include <cstring>
#include <deque>
#include <locale>
#include <map>

static const char *FX_NODE_NAME = "fxsound";
static const char *FX_NODE_DESC = "FxSound";
static const int FX_CHANNELS = 2;
static const int FX_MAX_BLOCK = 1024; // frames per DSP call

// ---------------------------------------------------------------- logging
static std::mutex g_log_mu;
static std::function<void(const std::string &)> g_log_cb;

void FxEngine::setLogCallback(std::function<void(const std::string &)> cb)
{
    std::lock_guard<std::mutex> l(g_log_mu);
    g_log_cb = std::move(cb);
}

void FxEngine::log(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::lock_guard<std::mutex> l(g_log_mu);
    if (g_log_cb) g_log_cb(buf);
    else fprintf(stderr, "[fxsound] %s\n", buf);
}

static std::wstring widen(const std::string &s)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> c;
    try { return c.from_bytes(s); } catch (...) { return std::wstring(s.begin(), s.end()); }
}
static std::string narrow(const std::wstring &s)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> c;
    try { return c.to_bytes(s); } catch (...) { return std::string(); }
}

// Extract "name" from a metadata JSON value like { "name": "alsa_output.pci..." }
static std::string json_name(const char *value)
{
    if (!value) return "";
    struct spa_json it[2];
    spa_json_init(&it[0], value, strlen(value));
    if (spa_json_enter_object(&it[0], &it[1]) <= 0) return "";
    char key[64];
    while (spa_json_get_string(&it[1], key, sizeof key) > 0) {
        char v[512];
        if (strcmp(key, "name") == 0) {
            if (spa_json_get_string(&it[1], v, sizeof v) > 0) return v;
            return "";
        }
        const char *dummy;
        if (spa_json_next(&it[1], &dummy) <= 0) break;
    }
    return "";
}

// ---------------------------------------------------------------- PipeWire state
struct FxPw {
    FxEngine *eng = nullptr;
    DfxDsp *dsp = nullptr;
    std::mutex *dsp_mu = nullptr;
    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    spa_hook core_listener{};
    pw_registry *registry = nullptr;
    spa_hook registry_listener{};
    pw_metadata *metadata = nullptr;
    spa_hook metadata_listener{};
    uint32_t metadata_id = SPA_ID_INVALID;

    pw_stream *capture = nullptr, *playback = nullptr;
    spa_hook capture_listener{}, playback_listener{};
    std::atomic<uint32_t> rate{48000};
    std::atomic<uint64_t> frames{0};
    std::atomic<double> load{0};
    std::atomic<uint64_t> n_capture{0}, n_playback{0}, n_no_input{0}, n_extra_input{0}, n_no_output{0}, n_trigger_fail{0};
    std::atomic<uint32_t> last_in_frames{0}, last_out_max{0}, last_requested{0}, max_in_frames{0};
    uint32_t playback_node_id = SPA_ID_INVALID;
    bool connected = false;

    // devices
    std::mutex dev_mu;
    std::map<uint32_t, FxOutputDevice> sinks;  // real sinks by global id
    std::string configured_default;            // default.configured.audio.sink (name)
    std::string chosen_output;                 // user choice ("" = automatic)
    std::string active_output;                 // what the playback stream targets now
    std::string restore_default;               // configured default before we claimed it
    bool claim = true, claimed = false;
    std::deque<std::chrono::steady_clock::time_point> reclaims;
    std::atomic<bool> muted{false};
    bool dirty_route = false;
    std::atomic<int> done_seq{-1};
    std::atomic<bool> broken{false};
};

// Round-trip to the server (and the session manager's reactions); call with the loop locked.
static void sync_wait(FxPw *pw, double timeout_s = 2.0)
{
    if (!pw->core) return;
    int seq = pw_core_sync(pw->core, PW_ID_CORE, 0);
    auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_s);
    while (pw->done_seq.load() != seq && std::chrono::steady_clock::now() < end)
        pw_thread_loop_timed_wait(pw->loop, 1);
}

// ---------------------------------------------------------------- audio processing
static void capture_process(void *d)
{
    FxPw *pw = (FxPw *)d;
    pw->n_capture++;
    if (pw_stream_trigger_process(pw->playback) < 0) {
        pw->n_trigger_fail++;
        // playback not ready: just recycle input buffers
        pw_buffer *b;
        while ((b = pw_stream_dequeue_buffer(pw->capture)) != nullptr) pw_stream_queue_buffer(pw->capture, b);
    }
}

static void playback_process(void *d)
{
    FxPw *pw = (FxPw *)d;
    pw->n_playback++;
    pw_buffer *in = nullptr, *t;
    // take the most recent capture buffer
    while ((t = pw_stream_dequeue_buffer(pw->capture)) != nullptr) {
        if (in) { pw_stream_queue_buffer(pw->capture, in); pw->n_extra_input++; }
        in = t;
    }
    if (!in) pw->n_no_input++;
    pw_buffer *out = pw_stream_dequeue_buffer(pw->playback);
    if (!out) { pw->n_no_output++; if (in) pw_stream_queue_buffer(pw->capture, in); return; }

    spa_data *od = &out->buffer->datas[0];
    float *dst = (float *)od->data;
    uint32_t stride = sizeof(float) * FX_CHANNELS;
    uint32_t max_frames = od->maxsize / stride;
    uint32_t n = 0;

    if (in) {
        spa_data *id = &in->buffer->datas[0];
        uint32_t off = SPA_MIN(id->chunk->offset, id->maxsize);
        uint32_t size = SPA_MIN(id->chunk->size, id->maxsize - off);
        n = SPA_MIN(size / stride, max_frames);
        pw->last_in_frames = size / stride; pw->last_out_max = max_frames; pw->last_requested = (uint32_t)out->requested;
        if (size / stride > pw->max_in_frames) pw->max_in_frames = size / stride;
        if (dst && id->data) memcpy(dst, SPA_PTROFF(id->data, off, void), n * stride);
    } else {
        n = out->requested ? SPA_MIN((uint32_t)out->requested, max_frames) : 0;
        if (dst) memset(dst, 0, n * stride);
    }

    if (dst && n) {
        auto t0 = std::chrono::steady_clock::now();
        uint32_t rate = pw->rate.load();
        {
            std::lock_guard<std::mutex> l(*pw->dsp_mu);
            for (uint32_t pos = 0; pos < n; pos += FX_MAX_BLOCK) {
                int blk = (int)SPA_MIN((uint32_t)FX_MAX_BLOCK, n - pos);
                short *p = (short *)(dst + pos * FX_CHANNELS);
                pw->dsp->setSignalFormat(32, FX_CHANNELS, (int)rate, 32);
                pw->dsp->processAudio(p, p, blk, 0);
            }
        }
        // never send NaN/Inf to the speakers
        if (pw->muted.load()) memset(dst, 0, n * stride);
        else for (uint32_t i = 0; i < n * FX_CHANNELS; i++)
            if (!std::isfinite(dst[i])) dst[i] = 0.0f;
        double spent = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        double budget = (double)n / rate;
        pw->load = pw->load * 0.95 + (spent / budget) * 0.05;
        pw->frames += n;
    }

    od->chunk->offset = 0;
    od->chunk->stride = stride;
    od->chunk->size = n * stride;
    if (in) pw_stream_queue_buffer(pw->capture, in);
    pw_stream_queue_buffer(pw->playback, out);
}

static void capture_param_changed(void *d, uint32_t id, const spa_pod *param)
{
    FxPw *pw = (FxPw *)d;
    if (id != SPA_PARAM_Format || !param) return;
    spa_audio_info_raw info{};
    if (spa_format_audio_raw_parse(param, &info) >= 0 && info.rate) {
        pw->rate = info.rate;
        FxEngine::log("audio format: %u Hz, %u channels", info.rate, info.channels);
    }
}

static void log_state(FxPw *pw, const char *which, pw_stream_state state, const char *error)
{
    FxEngine::log("%s: %s%s%s", which, pw_stream_state_as_string(state), error ? " - " : "", error ? error : "");
    if (pw->playback) {
        uint32_t nid = pw_stream_get_node_id(pw->playback);
        if (nid != SPA_ID_INVALID) pw->playback_node_id = nid;
    }
}
static void capture_state_changed(void *d, pw_stream_state, pw_stream_state state, const char *error)
{ log_state((FxPw *)d, "FxSound device", state, error); }
static void playback_state_changed(void *d, pw_stream_state, pw_stream_state state, const char *error)
{ log_state((FxPw *)d, "FxSound output", state, error); }

static const pw_stream_events capture_events = [] {
    pw_stream_events e{};
    e.version = PW_VERSION_STREAM_EVENTS;
    e.state_changed = capture_state_changed;
    e.param_changed = capture_param_changed;
    e.process = capture_process;
    return e;
}();

static const pw_stream_events playback_events = [] {
    pw_stream_events e{};
    e.version = PW_VERSION_STREAM_EVENTS;
    e.state_changed = playback_state_changed;
    e.process = playback_process;
    return e;
}();

static const spa_pod *format_param(spa_pod_builder *b)
{
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.channels = FX_CHANNELS;
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
    // rate left at 0: follow the graph rate, so there is no resampling inside FxSound
    return spa_format_audio_raw_build(b, SPA_PARAM_EnumFormat, &info);
}

// ---------------------------------------------------------------- routing (loop thread)
static std::string pick_output(FxPw *pw)
{
    std::lock_guard<std::mutex> l(pw->dev_mu);
    auto exists = [&](const std::string &n) {
        for (auto &kv : pw->sinks) if (kv.second.name == n) return true;
        return false;
    };
    if (!pw->chosen_output.empty() && exists(pw->chosen_output)) return pw->chosen_output;
    if (!pw->restore_default.empty() && exists(pw->restore_default)) return pw->restore_default;
    if (!pw->configured_default.empty() && pw->configured_default != FX_NODE_NAME && exists(pw->configured_default))
        return pw->configured_default;
    const FxOutputDevice *best = nullptr;
    for (auto &kv : pw->sinks)
        if (!best || kv.second.priority > best->priority) best = &kv.second;
    return best ? best->name : "";
}

static void connect_playback(FxPw *pw)
{
    std::string target = pick_output(pw);
    if (pw->connected && target == pw->active_output) return;
    if (pw->connected) pw_stream_disconnect(pw->playback);
    pw->active_output = target;
    pw_properties *p = pw_properties_new(nullptr, nullptr);
    pw_properties_set(p, PW_KEY_TARGET_OBJECT, target.empty() ? nullptr : target.c_str());
    pw_stream_update_properties(pw->playback, &p->dict);
    pw_properties_free(p);

    uint8_t buf[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof buf);
    const spa_pod *params[1] = {format_param(&b)};
    int res = pw_stream_connect(pw->playback, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                                (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                                  PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_TRIGGER),
                                params, 1);
    pw->connected = res >= 0;
    FxEngine::log("output device: %s%s", target.empty() ? "(none available)" : target.c_str(),
                  res < 0 ? " - connect failed" : "");
}

static void set_configured_default(FxPw *pw, const std::string &name)
{
    if (!pw->metadata) return;
    if (name.empty()) {
        pw_metadata_set_property(pw->metadata, PW_ID_CORE, "default.configured.audio.sink", nullptr, nullptr);
    } else {
        std::string v = "{ \"name\": \"" + name + "\" }";
        pw_metadata_set_property(pw->metadata, PW_ID_CORE, "default.configured.audio.sink", "Spa:String:JSON", v.c_str());
    }
}

static void notify(FxPw *pw)
{
    pw->eng->notifyDevicesChanged();
}

static void claim_default_now(FxPw *pw)
{
    if (!pw->claim || !pw->metadata) return;
    if (pw->configured_default == FX_NODE_NAME) {
        // already the default (left over from a previous run that did not exit cleanly)
        if (!pw->claimed) { pw->claimed = true; pw->restore_default.clear(); }
        return;
    }
    // Guard against fighting another program over the default device.
    auto now = std::chrono::steady_clock::now();
    while (!pw->reclaims.empty() && now - pw->reclaims.front() > std::chrono::seconds(10)) pw->reclaims.pop_front();
    if (pw->reclaims.size() >= 5) {
        FxEngine::log("another program keeps changing the default output; FxSound stops claiming it");
        pw->claim = false;
        return;
    }
    pw->reclaims.push_back(now);
    if (!pw->claimed) {
        pw->restore_default = pw->configured_default;
        pw->claimed = true;
    }
    set_configured_default(pw, FX_NODE_NAME);
}

static int metadata_property(void *d, uint32_t subject, const char *key, const char *type, const char *value)
{
    FxPw *pw = (FxPw *)d;
    (void)type;
    if (subject != PW_ID_CORE || !key || strcmp(key, "default.configured.audio.sink") != 0) return 0;
    std::string name = json_name(value);
    std::string prev = pw->configured_default;
    pw->configured_default = name;
    FxEngine::log("system default output: %s", name.empty() ? "(unset)" : name.c_str());
    if (name != FX_NODE_NAME && pw->claimed && pw->claim) {
        // The user picked another device in the system sound settings: play FxSound there
        // (exactly like FxSound on Windows follows the default device) and stay the default.
        bool real = false;
        {
            std::lock_guard<std::mutex> l(pw->dev_mu);
            for (auto &kv : pw->sinks) if (kv.second.name == name) real = true;
            if (real) { pw->chosen_output = name; pw->restore_default = name; }
        }
        if (real) connect_playback(pw);
        claim_default_now(pw);
        notify(pw);
    } else if (!pw->claimed && pw->claim && pw->capture) {
        claim_default_now(pw);
    }
    (void)prev;
    return 0;
}

static const pw_metadata_events metadata_events = [] {
    pw_metadata_events e{};
    e.version = PW_VERSION_METADATA_EVENTS;
    e.property = metadata_property;
    return e;
}();

static void registry_global(void *d, uint32_t id, uint32_t perms, const char *type, uint32_t version, const spa_dict *props)
{
    FxPw *pw = (FxPw *)d;
    (void)perms; (void)version;
    if (!props) return;
    if (strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
        const char *n = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
        if (n && strcmp(n, "default") == 0 && !pw->metadata) {
            pw->metadata = (pw_metadata *)pw_registry_bind(pw->registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0);
            pw->metadata_id = id;
            pw_metadata_add_listener(pw->metadata, &pw->metadata_listener, &metadata_events, pw);
        }
        return;
    }
    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0) return;
    const char *cls = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if (!cls || !name || strcmp(cls, "Audio/Sink") != 0 || strcmp(name, FX_NODE_NAME) == 0) return;
    FxOutputDevice dev;
    dev.id = id;
    dev.name = name;
    const char *desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    if (!desc) desc = spa_dict_lookup(props, PW_KEY_NODE_NICK);
    dev.description = desc ? desc : name;
    const char *prio = spa_dict_lookup(props, PW_KEY_PRIORITY_SESSION);
    dev.priority = prio ? atoi(prio) : 0;
    {
        std::lock_guard<std::mutex> l(pw->dev_mu);
        pw->sinks[id] = dev;
    }
    FxEngine::log("device added: %s (%s)", dev.description.c_str(), dev.name.c_str());
    pw->dirty_route = true;
    if (pw->capture) {
        // a device we prefer may have (re)appeared, e.g. USB headphones plugged back in
        connect_playback(pw);
        if (!pw->claimed && pw->claim) claim_default_now(pw);
    }
    notify(pw);
}

static void registry_global_remove(void *d, uint32_t id)
{
    FxPw *pw = (FxPw *)d;
    if (id == pw->metadata_id && pw->metadata) {
        spa_hook_remove(&pw->metadata_listener);
        pw_proxy_destroy((pw_proxy *)pw->metadata);
        pw->metadata = nullptr;
        pw->metadata_id = SPA_ID_INVALID;
        return;
    }
    bool was_sink = false;
    std::string name;
    {
        std::lock_guard<std::mutex> l(pw->dev_mu);
        auto it = pw->sinks.find(id);
        if (it != pw->sinks.end()) { was_sink = true; name = it->second.name; pw->sinks.erase(it); }
    }
    if (!was_sink) return;
    FxEngine::log("device removed: %s", name.c_str());
    if (pw->capture && name == pw->active_output) connect_playback(pw); // move to the next best device
    notify(pw);
}

static const pw_registry_events registry_events = [] {
    pw_registry_events e{};
    e.version = PW_VERSION_REGISTRY_EVENTS;
    e.global = registry_global;
    e.global_remove = registry_global_remove;
    return e;
}();

static void core_error(void *d, uint32_t id, int seq, int res, const char *message)
{
    FxPw *pw = (FxPw *)d;
    (void)seq;
    FxEngine::log("PipeWire error (id %u): %s %s", id, spa_strerror(res), message ? message : "");
    if (id == PW_ID_CORE && res == -EPIPE) {
        pw->broken = true; // connection to the server lost (PipeWire restarted?)
        pw_thread_loop_signal(pw->loop, false);
    }
}

static void core_done(void *d, uint32_t id, int seq)
{
    FxPw *pw = (FxPw *)d;
    if (id == PW_ID_CORE) { pw->done_seq = seq; pw_thread_loop_signal(pw->loop, false); }
}

static const pw_core_events core_events = [] {
    pw_core_events e{};
    e.version = PW_VERSION_CORE_EVENTS;
    e.error = core_error;
    e.done = core_done;
    return e;
}();

// ---------------------------------------------------------------- FxEngine
FxEngine::FxEngine(DfxDsp *external_dsp, std::mutex *external_mutex)
    : dsp_(external_dsp ? external_dsp : new DfxDsp()), owns_dsp_(external_dsp == nullptr),
      dsp_mu_(external_mutex ? external_mutex : &own_mu_) {}

FxEngine::~FxEngine()
{
    stop();
    if (owns_dsp_) delete dsp_;
}

void FxEngine::notifyDevicesChanged()
{
    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> l(cb_mu_);
        cb = devices_changed_;
    }
    if (cb) cb();
}

bool FxEngine::probe(std::string &error)
{
    static bool inited = false;
    if (!inited) { pw_init(nullptr, nullptr); inited = true; }
    pw_main_loop *ml = pw_main_loop_new(nullptr);
    if (!ml) { error = "cannot create PipeWire loop"; return false; }
    pw_context *ctx = pw_context_new(pw_main_loop_get_loop(ml), nullptr, 0);
    pw_core *core = ctx ? pw_context_connect(ctx, nullptr, 0) : nullptr;
    bool ok = core != nullptr;
    if (!ok) error = "cannot connect to the PipeWire sound server (" + std::string(strerror(errno)) + "). "
                     "FxSound for Linux needs PipeWire (with pipewire-pulse) to be running.";
    if (core) pw_core_disconnect(core);
    if (ctx) pw_context_destroy(ctx);
    pw_main_loop_destroy(ml);
    return ok;
}

bool FxEngine::start(std::string &error)
{
    if (pw_) return true;
    static bool inited = false;
    if (!inited) { pw_init(nullptr, nullptr); inited = true; }
    (void)inited;

    FxPw *pw = new FxPw();
    pw->eng = this;
    pw->dsp = dsp_;
    pw->dsp_mu = dsp_mu_;
    pw->muted = muted_.load();
    pw->loop = pw_thread_loop_new("fxsound-pw", nullptr);
    if (!pw->loop) { error = "cannot create PipeWire loop"; delete pw; return false; }
    pw->context = pw_context_new(pw_thread_loop_get_loop(pw->loop), nullptr, 0);
    if (!pw->context) { error = "cannot create PipeWire context"; pw_thread_loop_destroy(pw->loop); delete pw; return false; }
    if (pw_thread_loop_start(pw->loop) < 0) { error = "cannot start PipeWire thread"; pw_context_destroy(pw->context); pw_thread_loop_destroy(pw->loop); delete pw; return false; }

    pw_thread_loop_lock(pw->loop);
    pw->core = pw_context_connect(pw->context, nullptr, 0);
    if (!pw->core) {
        pw_thread_loop_unlock(pw->loop);
        error = "cannot connect to the PipeWire sound server. Is PipeWire running? (" + std::string(strerror(errno)) + ")";
        pw_thread_loop_stop(pw->loop);
        pw_context_destroy(pw->context);
        pw_thread_loop_destroy(pw->loop);
        delete pw;
        return false;
    }
    pw_core_add_listener(pw->core, &pw->core_listener, &core_events, pw);
    pw->registry = pw_core_get_registry(pw->core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(pw->registry, &pw->registry_listener, &registry_events, pw);

    // Wait until the initial registry contents (devices, default metadata) have arrived;
    // the 2nd round trip delivers the metadata properties after binding.
    sync_wait(pw, 3.0);
    sync_wait(pw, 3.0);

    // Capture side: the virtual "FxSound" output device applications play into.
    pw_properties *cp = pw_properties_new(
        PW_KEY_MEDIA_CLASS, "Audio/Sink",
        PW_KEY_NODE_NAME, FX_NODE_NAME,
        PW_KEY_NODE_DESCRIPTION, FX_NODE_DESC,
        PW_KEY_MEDIA_NAME, "FxSound",
        PW_KEY_NODE_GROUP, "fxsound.group",
        PW_KEY_NODE_LINK_GROUP, "fxsound.group",
        PW_KEY_NODE_VIRTUAL, "true",
        "device.icon-name", "audio-speakers",
        PW_KEY_APP_NAME, "FxSound",
        PW_KEY_APP_ICON_NAME, "fxsound",
        "audio.position", "[ FL FR ]",
        // keep running (and keep the reverb tail) only while something plays
        PW_KEY_NODE_PASSIVE, "true",
        nullptr);
    pw->capture = pw_stream_new(pw->core, "FxSound", cp);
    // Playback side: FxSound -> the real speakers/headphones.
    pw_properties *pp = pw_properties_new(
        PW_KEY_NODE_NAME, "fxsound_output",
        PW_KEY_NODE_DESCRIPTION, "FxSound output",
        PW_KEY_MEDIA_NAME, "FxSound output",
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_NODE_GROUP, "fxsound.group",
        PW_KEY_NODE_LINK_GROUP, "fxsound.group",
        PW_KEY_NODE_VIRTUAL, "true",
        PW_KEY_APP_NAME, "FxSound",
        PW_KEY_APP_ICON_NAME, "fxsound",
        "audio.position", "[ FL FR ]",
        PW_KEY_NODE_PASSIVE, "true",
        "node.dont-reconnect", "false",
        "node.dont-fallback", "false",
        // our own playback must never be routed into FxSound (loop)
        "node.exclusive", "false",
        nullptr);
    pw->playback = pw_stream_new(pw->core, "FxSound output", pp);
    if (!pw->capture || !pw->playback) {
        pw_thread_loop_unlock(pw->loop);
        error = "cannot create PipeWire streams";
        pw_ = pw;
        stop();
        return false;
    }
    pw_stream_add_listener(pw->capture, &pw->capture_listener, &capture_events, pw);
    pw_stream_add_listener(pw->playback, &pw->playback_listener, &playback_events, pw);

    // Connect playback first so it is active before the capture side triggers it (as module-loopback).
    connect_playback(pw);
    uint8_t buf[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof buf);
    const spa_pod *params[1] = {format_param(&b)};
    int res = pw_stream_connect(pw->capture, PW_DIRECTION_INPUT, PW_ID_ANY,
                                (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                                  PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_ASYNC),
                                params, 1);
    if (res < 0) {
        pw_thread_loop_unlock(pw->loop);
        error = std::string("cannot create the FxSound device: ") + spa_strerror(res);
        pw_ = pw;
        stop();
        return false;
    }
    if (pw->claim) claim_default_now(pw);
    pw_ = pw;
    pw_thread_loop_unlock(pw->loop);
    log("engine started (PipeWire %s)", pw_get_library_version());
    return true;
}

void FxEngine::stop()
{
    FxPw *pw = pw_;
    if (!pw) return;
    pw_thread_loop_lock(pw->loop);
    if (pw->claimed && pw->metadata) {
        // Give the system its previous default output back. If that is unknown (e.g. a previous
        // FxSound run was killed and left itself as the default), use the device we were playing to.
        std::string back = (pw->restore_default.empty() || pw->restore_default == FX_NODE_NAME) ? pw->active_output : pw->restore_default;
        set_configured_default(pw, back);
        pw->claimed = false;
        // make sure the metadata update reaches the server before we disconnect
        sync_wait(pw);
    }
    if (pw->capture) { pw_stream_destroy(pw->capture); pw->capture = nullptr; }
    if (pw->playback) { pw_stream_destroy(pw->playback); pw->playback = nullptr; }
    if (pw->metadata) { spa_hook_remove(&pw->metadata_listener); pw_proxy_destroy((pw_proxy *)pw->metadata); pw->metadata = nullptr; }
    if (pw->registry) { spa_hook_remove(&pw->registry_listener); pw_proxy_destroy((pw_proxy *)pw->registry); pw->registry = nullptr; }
    if (pw->core) { spa_hook_remove(&pw->core_listener); pw_core_disconnect(pw->core); pw->core = nullptr; }
    pw_thread_loop_unlock(pw->loop);
    pw_thread_loop_stop(pw->loop);
    if (pw->context) pw_context_destroy(pw->context);
    pw_thread_loop_destroy(pw->loop);
    delete pw;
    pw_ = nullptr;
    log("engine stopped");
}

bool FxEngine::isRunning() const { return pw_ != nullptr; }
bool FxEngine::connectionLost() const { return pw_ && pw_->broken.load(); }

// ---- DSP control
void FxEngine::powerOn(bool on) { std::lock_guard<std::mutex> l(*dsp_mu_); dsp_->powerOn(on); }
bool FxEngine::isPowerOn() { std::lock_guard<std::mutex> l(*dsp_mu_); return dsp_->isPowerOn(); }

bool FxEngine::loadPreset(const std::string &path)
{
    std::lock_guard<std::mutex> l(*dsp_mu_);
    if (dsp_->loadPreset(widen(path)) != 0) { log("failed to load preset %s", path.c_str()); return false; }
    // Same sequence as FxController::setPreset() in the Windows app (verified bit-identical).
    for (int e = 0; e < DfxDsp::NumEffects; e++)
        dsp_->setEffectValue((DfxDsp::Effect)e, dsp_->getEffectValue((DfxDsp::Effect)e) * 10);
    for (int b = 0; b < dsp_->getNumEqBands(); b++) {
        dsp_->setEqBandFrequency(b, dsp_->getEqBandFrequency(b));
        dsp_->setEqBandBoostCut(b, dsp_->getEqBandBoostCut(b));
    }
    return true;
}

bool FxEngine::savePreset(const std::string &name, const std::string &path)
{
    std::lock_guard<std::mutex> l(*dsp_mu_);
    return dsp_->savePreset(widen(name), widen(path)) == 0;
}

std::string FxEngine::presetName(const std::string &path)
{
    std::lock_guard<std::mutex> l(*dsp_mu_);
    return narrow(dsp_->getPresetInfo(widen(path)).name);
}

float FxEngine::effectValue(Effect e) { std::lock_guard<std::mutex> l(*dsp_mu_); return dsp_->getEffectValue((DfxDsp::Effect)e); }
void FxEngine::setEffectValue(Effect e, float v) { std::lock_guard<std::mutex> l(*dsp_mu_); dsp_->setEffectValue((DfxDsp::Effect)e, v); }
int FxEngine::numEqBands() { std::lock_guard<std::mutex> l(*dsp_mu_); return dsp_->getNumEqBands(); }
float FxEngine::eqBandFrequency(int b) { std::lock_guard<std::mutex> l(*dsp_mu_); return dsp_->getEqBandFrequency(b); }
void FxEngine::setEqBandFrequency(int b, float hz) { std::lock_guard<std::mutex> l(*dsp_mu_); dsp_->setEqBandFrequency(b, hz); }
void FxEngine::eqBandFrequencyRange(int b, float *lo, float *hi) { std::lock_guard<std::mutex> l(*dsp_mu_); dsp_->getEqBandFrequencyRange(b, lo, hi); }
float FxEngine::eqBandBoostCut(int b) { std::lock_guard<std::mutex> l(*dsp_mu_); return dsp_->getEqBandBoostCut(b); }
void FxEngine::setEqBandBoostCut(int b, float db) { std::lock_guard<std::mutex> l(*dsp_mu_); dsp_->setEqBandBoostCut(b, db); }
void FxEngine::eqOn(bool on) { std::lock_guard<std::mutex> l(*dsp_mu_); dsp_->eqOn(on); }
void FxEngine::spectrumBandValues(float *v, int n) { std::lock_guard<std::mutex> l(*dsp_mu_); dsp_->getSpectrumBandValues(v, n); }

// ---- routing
std::vector<FxOutputDevice> FxEngine::outputDevices()
{
    std::vector<FxOutputDevice> v;
    if (!pw_) return v;
    std::lock_guard<std::mutex> l(pw_->dev_mu);
    for (auto &kv : pw_->sinks) v.push_back(kv.second);
    std::sort(v.begin(), v.end(), [](const FxOutputDevice &a, const FxOutputDevice &b) { return a.description < b.description; });
    return v;
}

std::string FxEngine::outputDevice()
{
    if (!pw_) return "";
    pw_thread_loop_lock(pw_->loop);
    std::string s = pw_->active_output;
    pw_thread_loop_unlock(pw_->loop);
    return s;
}

void FxEngine::setOutputDevice(const std::string &node_name)
{
    if (!pw_) return;
    pw_thread_loop_lock(pw_->loop);
    {
        std::lock_guard<std::mutex> l(pw_->dev_mu);
        pw_->chosen_output = node_name;
    }
    connect_playback(pw_);
    pw_thread_loop_unlock(pw_->loop);
}

void FxEngine::claimDefault(bool claim)
{
    if (!pw_) return;
    pw_thread_loop_lock(pw_->loop);
    pw_->claim = claim;
    pw_->reclaims.clear();
    if (claim) claim_default_now(pw_);
    else if (pw_->claimed) {
        set_configured_default(pw_, (pw_->restore_default.empty() || pw_->restore_default == FX_NODE_NAME) ? pw_->active_output : pw_->restore_default);
        pw_->claimed = false;
    }
    pw_thread_loop_unlock(pw_->loop);
}

void FxEngine::setDevicesChangedCallback(std::function<void()> cb)
{
    std::lock_guard<std::mutex> l(cb_mu_);
    devices_changed_ = std::move(cb);
}

void FxEngine::mute(bool m) { muted_ = m; if (pw_) pw_->muted = m; }
bool FxEngine::isMuted() const { return muted_; }

bool FxEngine::isDefaultClaimed()
{
    if (!pw_) return false;
    pw_thread_loop_lock(pw_->loop);
    bool c = pw_->claimed && pw_->configured_default == FX_NODE_NAME;
    pw_thread_loop_unlock(pw_->loop);
    return c;
}

std::string FxEngine::systemDefaultOutput()
{
    if (!pw_) return "";
    pw_thread_loop_lock(pw_->loop);
    std::string s;
    if (pw_->claimed) s = pw_->restore_default;
    else if (pw_->configured_default != FX_NODE_NAME) s = pw_->configured_default;
    if (s.empty() || s == FX_NODE_NAME) s = pw_->active_output;
    pw_thread_loop_unlock(pw_->loop);
    return s;
}

uint32_t FxEngine::sampleRate() const { return pw_ ? pw_->rate.load() : 0; }
uint64_t FxEngine::framesProcessed() const { return pw_ ? pw_->frames.load() : 0; }
double FxEngine::cpuLoad() const { return pw_ ? pw_->load.load() : 0; }
std::string FxEngine::debugStats() const
{
    if (!pw_) return "";
    char b[256];
    snprintf(b, sizeof b, "capture %llu playback %llu no_input %llu extra_input %llu no_output %llu trigger_fail %llu in %u max_in %u outmax %u req %u",
             (unsigned long long)pw_->n_capture, (unsigned long long)pw_->n_playback, (unsigned long long)pw_->n_no_input,
             (unsigned long long)pw_->n_extra_input, (unsigned long long)pw_->n_no_output, (unsigned long long)pw_->n_trigger_fail, pw_->last_in_frames.load(), pw_->max_in_frames.load(), pw_->last_out_max.load(), pw_->last_requested.load());
    return b;
}
