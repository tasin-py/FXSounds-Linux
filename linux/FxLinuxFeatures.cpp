/*
 * FxLinuxFeatures.cpp - see FxLinuxFeatures.h.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "FxLinuxFeatures.h"
#include "FxLinuxLog.h"
#include "FxController.h"
#include "FxModel.h"
#include "FxMessage.h"
#include <csignal>
#include <thread>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>


namespace FxLinux
{
// ---------------------------------------------------------------- configuration
// GitHub repository of the Linux port ("owner/name"); the update check uses its releases.
const char *UPDATE_REPO = "tasin-py/FXSounds-Linux";
const char *PORT_DONATE_URL = "https://www.supportkori.com/tasinpy";
const char *CORE_DONATE_URL = "https://www.paypal.com/donate/?hosted_button_id=JVNQGYXCQ2GPG";
static const char *PORT_DEVELOPER_NAME = "Tasin";

// ---------------------------------------------------------------- web links
// xdg-open exits with 0 even when nothing can open https links, so ask xdg-mime whether any
// application is registered for them ($BROWSER also counts).
static bool hasBrowser()
{
    if (juce::SystemStats::getEnvironmentVariable("BROWSER", "").isNotEmpty()) return true;
    juce::ChildProcess p;
    if (!p.start(juce::StringArray{"xdg-mime", "query", "default", "x-scheme-handler/https"}, juce::ChildProcess::wantStdOut))
        return false; // no xdg-utils: launchInDefaultBrowser cannot work either
    auto out = p.readAllProcessOutput().trim();
    p.waitForProcessToFinish(3000);
    return out.isNotEmpty();
}

void openUrl(const juce::String &url)
{
    bool browser = hasBrowser();
    bool started = browser && juce::URL(url).launchInDefaultBrowser();
    FxLinuxLog::write("open link " + url + (started ? "" : browser ? " - failed to start the browser" : " - no web browser found"));
    if (!started) {
        juce::SystemClipboard::copyTextToClipboard(url);
        FxConfirmationMessage::showMessage(TRANS("No web browser was found to open this link.") + "\n" +
                                           TRANS("The link was copied to the clipboard:") + "\n" + url,
                                           FxConfirmationMessage::Style::OK);
    }
}

// ---------------------------------------------------------------- donate
void addDonateItems(juce::PopupMenu &menu)
{
    menu.addSectionHeader(TRANS("Who would you like to support?"));
    menu.addItem(TRANS("Support the Linux port developer") + " - " + PORT_DEVELOPER_NAME + " (supportkori.com)",
                 []() { openUrl(PORT_DONATE_URL); });
    menu.addItem(TRANS("Support the FxSound core developers") + " - " + TRANS("FxSound team, original app") + " (PayPal)",
                 []() { openUrl(CORE_DONATE_URL); });
}

void showDonateMenu(juce::Component *target)
{
    juce::PopupMenu menu;
    addDonateItems(menu);
    auto opts = juce::PopupMenu::Options();
    if (target != nullptr) opts = opts.withTargetComponent(target);
    menu.showMenuAsync(opts);
}

// ---------------------------------------------------------------- update check
static juce::Array<int> versionNumbers(const juce::String &v)
{
    juce::Array<int> n;
    juce::String cur;
    for (auto c : v + " ") {
        if (juce::CharacterFunctions::isDigit(c)) cur << juce::String::charToString(c);
        else if (cur.isNotEmpty()) { n.add(cur.getIntValue()); cur.clear(); }
    }
    return n;
}

static bool isNewer(const juce::String &remote, const juce::String &local)
{
    auto a = versionNumbers(remote), b = versionNumbers(local);
    for (int i = 0; i < juce::jmax(a.size(), b.size()); i++) {
        int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x > y;
    }
    return false;
}

void checkForUpdates(bool interactive)
{
    // FXSOUND_UPDATE_REPO=owner/name overrides the built-in repository (testing, forks).
    juce::String repo = juce::SystemStats::getEnvironmentVariable("FXSOUND_UPDATE_REPO", UPDATE_REPO);
    if (repo.isEmpty()) {
        FxLinuxLog::write("update check: no update repository configured");
        if (interactive)
            FxConfirmationMessage::showMessage(TRANS("You are using FxSound for Linux") + " " + juce::JUCEApplication::getInstance()->getApplicationVersion() + ".\n" +
                                               TRANS("Update checking is not configured in this build yet."), FxConfirmationMessage::Style::OK);
        return;
    }
    juce::Thread::launch([repo, interactive]() {
        // 1) https://github.com/<repo>/releases/latest redirects to .../releases/tag/<tag>. Not rate limited,
        //    unlike the REST API (60 requests/hour per IP - shared by everyone behind the same ISP NAT).
        int status = 0;
        juce::String tag, page;
        {
            juce::StringPairArray headers;
            auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs(10000)
                            .withNumRedirectsToFollow(0)
                            .withResponseHeaders(&headers)
                            .withStatusCode(&status)
                            .withExtraHeaders("User-Agent: FxSound-Linux-Updater");
            std::unique_ptr<juce::InputStream> in(juce::URL("https://github.com/" + repo + "/releases/latest").createInputStream(opts));
            juce::String location;
            for (auto &key : headers.getAllKeys())
                if (key.equalsIgnoreCase("location")) location = headers[key].trim();
            if (location.contains("/releases/tag/")) {
                tag = juce::URL::removeEscapeChars(location.fromLastOccurrenceOf("/releases/tag/", false, false)
                                                       .upToFirstOccurrenceOf("?", false, false));
                page = "https://github.com/" + repo + "/releases/tag/" + tag;
            }
        }
        // 2) fallback: REST API
        if (tag.isEmpty()) {
            auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs(10000)
                            .withStatusCode(&status)
                            .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: FxSound-Linux-Updater");
            std::unique_ptr<juce::InputStream> in(juce::URL("https://api.github.com/repos/" + repo + "/releases/latest").createInputStream(opts));
            auto json = juce::JSON::parse(in ? in->readEntireStreamAsString() : juce::String());
            tag = json.getProperty("tag_name", "").toString();
            page = json.getProperty("html_url", "").toString();
        }
        juce::MessageManager::callAsync([tag, page, status, interactive]() {
            auto local = juce::JUCEApplication::getInstance()->getApplicationVersion();
            if (tag.isEmpty()) {
                FxLinuxLog::write("update check failed (HTTP " + juce::String(status) + ")");
                if (interactive)
                    FxConfirmationMessage::showMessage(TRANS("Could not check for updates.") + "\n" +
                                                       TRANS("Please check your internet connection and try again."), FxConfirmationMessage::Style::OK);
                return;
            }
            FxLinuxLog::write("update check: latest " + tag + ", installed " + local);
            if (isNewer(tag, local)) {
                if (FxConfirmationMessage::showMessage(TRANS("A new version of FxSound is available:") + " " + tag + "\n" +
                                                       TRANS("You have") + " " + local + ". " + TRANS("Open the download page?"),
                                                       FxConfirmationMessage::Style::YesNo))
                    openUrl(page);
            } else if (interactive) {
                FxConfirmationMessage::showMessage(TRANS("FxSound is up to date.") + " (" + local + ")", FxConfirmationMessage::Style::OK);
            }
        });
    });
}

// ---------------------------------------------------------------- copy log
void copyLogToClipboard()
{
    auto *ap = FxController::getInstance().getAudioPassthru();
    juce::String diag = ap ? juce::String(ap->diagnostics()) : juce::String("engine: not created\n");
    auto text = FxLinuxLog::systemReport(diag) + FxLinuxLog::readAll();
    juce::SystemClipboard::copyTextToClipboard(text);
    auto report = FxLinuxLog::logFile().getSiblingFile("fxsound-report.txt");
    report.replaceWithText(text);
    FxLinuxLog::write("log copied to clipboard (" + juce::String(text.length()) + " characters)");
    FxModel::getModel().pushMessage(TRANS("Log copied to the clipboard.") + "\r\n" + TRANS("Paste it into your bug report.") + " ",
                                    { TRANS("Open the report file"), juce::URL(report).toString(false) });
}

// ---------------------------------------------------------------- autostart
// JUCE derives currentExecutableFile from argv[0], which breaks for a relative argv[0] once the app has
// changed its working directory; the kernel's /proc/self/exe link is always right.
static juce::File executableFile()
{
    auto link = juce::File("/proc/self/exe").getLinkedTarget();
    if (link.existsAsFile()) return link;
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile);
}

static juce::File autostartFile()
{
    auto cfg = juce::SystemStats::getEnvironmentVariable("XDG_CONFIG_HOME", "");
    juce::File base = cfg.isNotEmpty() ? juce::File(cfg) : juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".config");
    return base.getChildFile("autostart").getChildFile("fxsound.desktop");
}

bool isAutostartEnabled()
{
    auto f = autostartFile();
    return f.existsAsFile() && !f.loadFileAsString().contains("Hidden=true");
}

void setAutostartEnabled(bool enable)
{
    auto f = autostartFile();
    if (!enable) { f.deleteFile(); return; }
    // An AppImage must be started through the AppImage file, not the mounted squashfs path.
    auto exe = juce::SystemStats::getEnvironmentVariable("APPIMAGE", "");
    if (exe.isEmpty()) exe = executableFile().getFullPathName();
    f.getParentDirectory().createDirectory();
    f.replaceWithText("[Desktop Entry]\n"
                      "Type=Application\n"
                      "Name=FxSound\n"
                      "Comment=Audio enhancer\n"
                      "Exec=\"" + exe + "\" --run_minimized\n"
                      "Icon=fxsound\n"
                      "Terminal=false\n"
                      "X-GNOME-Autostart-enabled=true\n"
                      "X-GNOME-Autostart-Delay=3\n");
}

juce::File getAppDataDir()
{
    auto exe_dir = executableFile().getParentDirectory();
    if (exe_dir.getChildFile("Factsoft").isDirectory()) return exe_dir;
    auto share = exe_dir.getParentDirectory().getChildFile("share").getChildFile("fxsound");
    if (share.getChildFile("Factsoft").isDirectory()) return share;
    return exe_dir;
}

namespace
{
    volatile sig_atomic_t g_quit_signal = 0;
    void onQuitSignal(int sig) { g_quit_signal = sig; }

    class QuitSignalWatcher : public juce::Timer
    {
    public:
        void timerCallback() override
        {
            if (g_quit_signal == 0) return;
            FxLinuxLog::write("received signal " + juce::String((int)g_quit_signal) + ", quitting");
            g_quit_signal = 0;
            stopTimer();
            FxController::getInstance().exit();
        }
    };
}

// ---------------------------------------------------------------- single instance
static std::string runtimeDir()
{
    const char *xdg = getenv("XDG_RUNTIME_DIR");
    if (xdg != nullptr && *xdg != '\0' && access(xdg, W_OK) == 0) return xdg;
    std::string d = "/tmp/fxsound-" + std::to_string(getuid());
    mkdir(d.c_str(), 0700);
    return d;
}

bool claimSingleInstance(const juce::String &commandline)
{
    const std::string dir = runtimeDir();
    const std::string lock_path = dir + "/fxsound.lock", sock_path = dir + "/fxsound.sock";
    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    if (sock_path.size() >= sizeof(addr.sun_path)) return true; // cannot happen with sane paths; run anyway
    memcpy(addr.sun_path, sock_path.c_str(), sock_path.size() + 1);

    int lock_fd = open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lock_fd >= 0 && flock(lock_fd, LOCK_EX | LOCK_NB) != 0 && errno == EWOULDBLOCK) {
        // FxSound is already running for this user: hand it our command line (shows its window)
        int c = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (c >= 0 && connect(c, (sockaddr *)&addr, sizeof(addr)) == 0) {
            auto msg = commandline.toStdString();
            (void)!write(c, msg.data(), msg.size());
        }
        if (c >= 0) close(c);
        FxLinuxLog::write("FxSound is already running - showing that window instead");
        return false;
    }
    // first instance (the lock fd stays open for the life of the process)
    unlink(sock_path.c_str());
    int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0 || bind(s, (sockaddr *)&addr, sizeof(addr)) != 0 || listen(s, 4) != 0) {
        FxLinuxLog::write("single instance: cannot listen on " + juce::String(sock_path) + ": " + juce::String(strerror(errno)));
        if (s >= 0) close(s);
        return true;
    }
    std::thread([s]() {
        for (;;) {
            int c = accept4(s, nullptr, nullptr, SOCK_CLOEXEC);
            if (c < 0) { if (errno == EINTR) continue; break; }
            std::string msg;
            char buf[4096];
            ssize_t n;
            while ((n = read(c, buf, sizeof buf)) > 0 || (n < 0 && errno == EINTR))
                if (n > 0) msg.append(buf, (size_t)n);
            close(c);
            auto cmd = juce::String::fromUTF8(msg.c_str());
            juce::MessageManager::callAsync([cmd]() {
                FxLinuxLog::write("another start of FxSound" + (cmd.isEmpty() ? juce::String() : " (" + cmd + ")"));
                FxController::getInstance().applyConfig(cmd);
            });
        }
    }).detach();
    return true;
}

void installQuitSignalHandlers()
{
    static QuitSignalWatcher watcher;
    struct sigaction sa {};
    sa.sa_handler = onQuitSignal;
    sigemptyset(&sa.sa_mask);
    for (int s : {SIGTERM, SIGINT, SIGHUP}) sigaction(s, &sa, nullptr);
    watcher.startTimer(100);
}

void sortFilesLikeWindows(juce::Array<juce::File> &files)
{
    struct ByName
    {
        static int compareElements(const juce::File &a, const juce::File &b)
        {
            return a.getFileName().compareIgnoreCase(b.getFileName());
        }
    } cmp;
    files.sort(cmp, true);
}

bool hasDisplay()
{
    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr) return true;
    auto msg = "FxSound needs a graphical desktop session, but no display could be opened (DISPLAY=\""
             + juce::SystemStats::getEnvironmentVariable("DISPLAY", "") + "\"). Start FxSound from your desktop.";
    FxLinuxLog::write(msg);
    fprintf(stderr, "%s\n", msg.toRawUTF8());
    return false;
}

bool isWayland()
{
    return juce::SystemStats::getEnvironmentVariable("XDG_SESSION_TYPE", "") == "wayland"
        || juce::SystemStats::getEnvironmentVariable("WAYLAND_DISPLAY", "").isNotEmpty();
}

void fx_hotkey_log(const juce::String &m) { FxLinuxLog::write(m); }
std::function<void(int)> fx_hotkey_default_handler()
{
    return [](int hk) { FxController::getInstance().handleHotkey((unsigned int)hk); };
}
}
