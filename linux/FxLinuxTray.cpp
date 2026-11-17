/*
 * FxLinuxTray.cpp - see FxLinuxTray.h.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "FxLinuxTray.h"
#include "FxLinuxLog.h"
#include "FxTheme.h"
#include "FxLinuxIconData.h"
#include <dlfcn.h>

// ---------------------------------------------------------------- icons
static juce::Image loadPng(const void *data, size_t size)
{
    return juce::ImageFileFormat::loadFrom(data, size);
}

static const char *iconName(bool power, bool processing)
{
    if (!power) return "fxsound-gray";
    if (!processing) return "fxsound-white";
    return FxTheme::getThemeMode() == FxThemeMode::Dark ? "fxsound-red" : "fxsound-blue";
}

juce::Image FxLinuxTray::iconImage(bool power, bool processing)
{
    if (!power) return loadPng(FxLinuxIconData::gray_logo_png, FxLinuxIconData::gray_logo_pngSize);
    if (!processing) return loadPng(FxLinuxIconData::white_logo_png, FxLinuxIconData::white_logo_pngSize);
    if (FxTheme::getThemeMode() == FxThemeMode::Dark) return loadPng(FxLinuxIconData::red_logo_png, FxLinuxIconData::red_logo_pngSize);
    return loadPng(FxLinuxIconData::blue_logo_png, FxLinuxIconData::blue_logo_pngSize);
}

// Icons are handed to the tray host as files in a private icon theme directory.
static juce::File iconDir()
{
    auto rt = juce::SystemStats::getEnvironmentVariable("XDG_RUNTIME_DIR", "");
    juce::File base = rt.isNotEmpty() ? juce::File(rt) : juce::File::getSpecialLocation(juce::File::tempDirectory);
    return base.getChildFile("fxsound-icons");
}

static void writeIcons()
{
    auto dir = iconDir();
    dir.createDirectory();
    struct { const char *n; const void *d; int s; } icons[] = {
        {"fxsound-gray", FxLinuxIconData::gray_logo_png, FxLinuxIconData::gray_logo_pngSize},
        {"fxsound-white", FxLinuxIconData::white_logo_png, FxLinuxIconData::white_logo_pngSize},
        {"fxsound-red", FxLinuxIconData::red_logo_png, FxLinuxIconData::red_logo_pngSize},
        {"fxsound-blue", FxLinuxIconData::blue_logo_png, FxLinuxIconData::blue_logo_pngSize},
    };
    for (auto &i : icons) {
        auto f = dir.getChildFile(juce::String(i.n) + ".png");
        if (f.getSize() != i.s) f.replaceWithData(i.d, (size_t)i.s);
    }
}

// ---------------------------------------------------------------- AppIndicator via dlopen
namespace
{
    typedef void *gp;
    struct Gtk {
        void *gtk = nullptr, *ind = nullptr, *gio = nullptr;
        int (*gtk_init_check)(int *, char ***);
        gp (*gtk_menu_new)();
        gp (*gtk_menu_item_new_with_label)(const char *);
        gp (*gtk_check_menu_item_new_with_label)(const char *);
        void (*gtk_check_menu_item_set_active)(gp, int);
        void (*gtk_check_menu_item_set_draw_as_radio)(gp, int);
        gp (*gtk_separator_menu_item_new)();
        void (*gtk_menu_item_set_submenu)(gp, gp);
        void (*gtk_menu_shell_append)(gp, gp);
        void (*gtk_widget_show_all)(gp);
        void (*gtk_widget_set_sensitive)(gp, int);
        void (*gtk_widget_destroy)(gp);
        unsigned long (*g_signal_connect_data)(gp, const char *, void (*)(), gp, gp, int);
        int (*g_main_context_iteration)(gp, int);
        gp (*app_indicator_new)(const char *, const char *, int);
        void (*app_indicator_set_status)(gp, int);
        void (*app_indicator_set_menu)(gp, gp);
        void (*app_indicator_set_icon_full)(gp, const char *, const char *);
        void (*app_indicator_set_icon_theme_path)(gp, const char *);
        void (*app_indicator_set_title)(gp, const char *);
        void (*app_indicator_set_secondary_activate_target)(gp, gp);
        gp (*g_bus_get_sync)(int, gp, gp);
        gp (*g_dbus_connection_call_sync)(gp, const char *, const char *, const char *, const char *, gp, gp, int, int, gp, gp);
        gp (*g_variant_new)(const char *, ...);
        void (*g_variant_get)(gp, const char *, ...);
        void (*g_variant_unref)(gp);
        void (*g_object_unref)(gp);

        template <typename T> bool sym(void *lib, T &fn, const char *name)
        {
            fn = (T)dlsym(lib, name);
            return fn != nullptr;
        }

        bool load()
        {
            if (std::getenv("FX_NO_APPINDICATOR")) return false;
            for (auto n : {"libayatana-appindicator3.so.1", "libappindicator3.so.1"})
                if ((ind = dlopen(n, RTLD_NOW | RTLD_GLOBAL)) != nullptr) break;
            if (!ind) return false;
            gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_GLOBAL);
            gio = dlopen("libgio-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
            if (!gtk || !gio) return false;
            bool ok = sym(gtk, gtk_init_check, "gtk_init_check") && sym(gtk, gtk_menu_new, "gtk_menu_new")
                && sym(gtk, gtk_menu_item_new_with_label, "gtk_menu_item_new_with_label")
                && sym(gtk, gtk_check_menu_item_new_with_label, "gtk_check_menu_item_new_with_label")
                && sym(gtk, gtk_check_menu_item_set_active, "gtk_check_menu_item_set_active")
                && sym(gtk, gtk_check_menu_item_set_draw_as_radio, "gtk_check_menu_item_set_draw_as_radio")
                && sym(gtk, gtk_separator_menu_item_new, "gtk_separator_menu_item_new")
                && sym(gtk, gtk_menu_item_set_submenu, "gtk_menu_item_set_submenu")
                && sym(gtk, gtk_menu_shell_append, "gtk_menu_shell_append")
                && sym(gtk, gtk_widget_show_all, "gtk_widget_show_all")
                && sym(gtk, gtk_widget_set_sensitive, "gtk_widget_set_sensitive")
                && sym(gtk, gtk_widget_destroy, "gtk_widget_destroy")
                && sym(gio, g_signal_connect_data, "g_signal_connect_data")
                && sym(gio, g_main_context_iteration, "g_main_context_iteration")
                && sym(gio, g_bus_get_sync, "g_bus_get_sync")
                && sym(gio, g_dbus_connection_call_sync, "g_dbus_connection_call_sync")
                && sym(gio, g_variant_new, "g_variant_new") && sym(gio, g_variant_get, "g_variant_get")
                && sym(gio, g_variant_unref, "g_variant_unref") && sym(gio, g_object_unref, "g_object_unref")
                && sym(ind, app_indicator_new, "app_indicator_new")
                && sym(ind, app_indicator_set_status, "app_indicator_set_status")
                && sym(ind, app_indicator_set_menu, "app_indicator_set_menu")
                && sym(ind, app_indicator_set_icon_full, "app_indicator_set_icon_full")
                && sym(ind, app_indicator_set_icon_theme_path, "app_indicator_set_icon_theme_path")
                && sym(ind, app_indicator_set_title, "app_indicator_set_title");
            sym(ind, app_indicator_set_secondary_activate_target, "app_indicator_set_secondary_activate_target");
            return ok;
        }

        // Is a StatusNotifierItem host running (KDE, GNOME AppIndicator extension, ...)?
        bool hostPresent()
        {
            gp bus = g_bus_get_sync(2 /*G_BUS_TYPE_SESSION*/, nullptr, nullptr);
            if (!bus) return false;
            gp r = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                               "NameHasOwner", g_variant_new("(s)", "org.kde.StatusNotifierWatcher"),
                                               nullptr, 0, 2000, nullptr, nullptr);
            int has = 0;
            if (r) { g_variant_get(r, "(b)", &has); g_variant_unref(r); }
            g_object_unref(bus);
            return has != 0;
        }
    };
}

struct FxLinuxTray::Impl : public juce::Timer
{
    MenuBuilder builder;
    std::function<void()> on_activate;
    Gtk g;
    bool use_indicator = false;
    bool host = false;
    gp indicator = nullptr;
    gp menu = nullptr;
    std::vector<std::function<void()>> actions;
    juce::String backend = "none";

    // JUCE XEmbed fallback
    struct JuceTray : public juce::SystemTrayIconComponent
    {
        Impl &owner;
        explicit JuceTray(Impl &o) : owner(o) {}
        void mouseDown(const juce::MouseEvent &e) override
        {
            if (e.mods.isPopupMenu()) {
                juce::PopupMenu m;
                owner.builder(m);
                m.showMenuAsync(juce::PopupMenu::Options());
            } else if (owner.on_activate) owner.on_activate();
        }
    };
    std::unique_ptr<JuceTray> juce_tray;

    static void onActivate(gp, gp data)
    {
        auto *fn = (std::function<void()> *)data;
        // run after GTK finished dispatching (the action may rebuild the menu)
        auto copy = *fn;
        juce::MessageManager::callAsync([copy]() { copy(); });
    }

    void addItems(gp shell, const juce::PopupMenu &pm)
    {
        for (juce::PopupMenu::MenuItemIterator it(pm); it.next();) {
            auto &item = it.getItem();
            gp w = nullptr;
            if (item.isSeparator) {
                w = g.gtk_separator_menu_item_new();
            } else if (item.isSectionHeader) {
                w = g.gtk_menu_item_new_with_label(item.text.toRawUTF8());
                g.gtk_widget_set_sensitive(w, 0);
            } else if (item.subMenu != nullptr) {
                w = g.gtk_menu_item_new_with_label(item.text.toRawUTF8());
                gp sub = g.gtk_menu_new();
                addItems(sub, *item.subMenu);
                g.gtk_menu_item_set_submenu(w, sub);
                g.gtk_widget_set_sensitive(w, item.isEnabled ? 1 : 0);
            } else {
                if (item.isTicked) {
                    w = g.gtk_check_menu_item_new_with_label(item.text.toRawUTF8());
                    g.gtk_check_menu_item_set_draw_as_radio(w, 0);
                    g.gtk_check_menu_item_set_active(w, 1); // before connecting: set_active emits "activate"
                } else {
                    w = g.gtk_menu_item_new_with_label(item.text.toRawUTF8());
                }
                g.gtk_widget_set_sensitive(w, item.isEnabled ? 1 : 0);
                if (item.action) {
                    actions.push_back(item.action);
                }
            }
            g.gtk_menu_shell_append(shell, w);
            if (!item.isSeparator && !item.isSectionHeader && item.subMenu == nullptr && item.action)
                pending_connect.push_back(w);
        }
    }
    std::vector<gp> pending_connect;

    void rebuild()
    {
        if (!use_indicator || !indicator) return;
        juce::PopupMenu pm;
        builder(pm);
        gp old = menu;
        actions.clear();
        actions.reserve(256);
        pending_connect.clear();
        menu = g.gtk_menu_new();
        addItems(menu, pm);
        // connect after all actions are stored so the vector never reallocates under the pointers
        for (size_t i = 0; i < pending_connect.size() && i < actions.size(); i++)
            g.g_signal_connect_data(pending_connect[i], "activate", (void (*)())onActivate, &actions[i], nullptr, 0);
        g.gtk_widget_show_all(menu);
        g.app_indicator_set_menu(indicator, menu);
        if (old) g.gtk_widget_destroy(old);
    }

    void timerCallback() override
    {
        // pump the GLib main context (D-Bus traffic of the indicator)
        for (int i = 0; i < 50 && g.g_main_context_iteration(nullptr, 0); i++) {}
        if (++host_check % 100 == 0) { // every ~5 s
            bool h = g.hostPresent();
            if (h != host) { host = h; FxLinuxLog::write(juce::String("tray host ") + (h ? "appeared" : "disappeared")); }
        }
    }
    int host_check = 0;
};

FxLinuxTray::FxLinuxTray(MenuBuilder builder, std::function<void()> on_activate) : impl_(new Impl())
{
    auto &m = *impl_;
    m.builder = std::move(builder);
    m.on_activate = std::move(on_activate);
    writeIcons();

    if (m.g.load() && m.g.gtk_init_check(nullptr, nullptr)) {
        m.use_indicator = true;
        m.indicator = m.g.app_indicator_new("fxsound", "fxsound-white", 0 /*APPLICATION_STATUS*/);
        m.g.app_indicator_set_icon_theme_path(m.indicator, iconDir().getFullPathName().toRawUTF8());
        m.g.app_indicator_set_title(m.indicator, "FxSound");
        m.g.app_indicator_set_status(m.indicator, 1 /*ACTIVE*/);
        m.host = m.g.hostPresent();
        m.backend = "AppIndicator";
        m.rebuild();
        m.startTimer(50);
    } else {
        m.juce_tray = std::make_unique<Impl::JuceTray>(m);
        m.juce_tray->setIconImage(iconImage(true, false), iconImage(true, false));
        m.juce_tray->setIconTooltip("FxSound");
        m.backend = "XEmbed";
        m.host = std::getenv("DISPLAY") != nullptr;
    }
    FxLinuxLog::write("tray: " + m.backend + (m.host ? " (tray host found)" : " (no tray host found)"));
}

FxLinuxTray::~FxLinuxTray()
{
    auto &m = *impl_;
    m.stopTimer();
    if (m.indicator) m.g.app_indicator_set_status(m.indicator, 0 /*PASSIVE*/);
    for (int i = 0; m.use_indicator && i < 20 && m.g.g_main_context_iteration(nullptr, 0); i++) {}
}

void FxLinuxTray::setStatus(bool power, bool processing, const juce::String &tooltip)
{
    auto &m = *impl_;
    if (m.use_indicator && m.indicator) {
        m.g.app_indicator_set_icon_full(m.indicator, iconName(power, processing), tooltip.toRawUTF8());
        m.g.app_indicator_set_title(m.indicator, tooltip.toRawUTF8());
        m.rebuild();
    } else if (m.juce_tray) {
        auto img = iconImage(power, processing);
        m.juce_tray->setIconImage(img, img);
        m.juce_tray->setIconTooltip(tooltip);
    }
}

void FxLinuxTray::refreshMenu() { impl_->rebuild(); }
bool FxLinuxTray::isAvailable() const { return impl_->host; }
juce::String FxLinuxTray::backendName() const { return impl_->backend; }
