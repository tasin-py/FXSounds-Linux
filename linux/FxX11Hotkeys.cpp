/*
 * FxX11Hotkeys.cpp - global hotkeys on X11 (XGrabKey on the root window).
 * Kept apart from the JUCE-heavy files: Xlib's Font/Time/Drawable clash with juce:: names.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#undef None
#undef Bool
#undef Status
#undef KeyPress
#define DONT_SET_USING_JUCE_NAMESPACE 1
#include <juce_events/juce_events.h>
#include <functional>
#include <map>
#ifndef KeyPress
#define FX_X11_KEYPRESS 2
#endif

namespace FxLinux
{
bool isWayland();
void fx_hotkey_log(const juce::String &m);
std::function<void(int)> fx_hotkey_default_handler();

// ---------------------------------------------------------------- X11 global hotkeys
namespace Hotkeys
{
namespace
{
    struct Grab { unsigned int mods; int keycode; };

    class X11Hotkeys : private juce::Timer
    {
    public:
        static X11Hotkeys &get() { static X11Hotkeys h; return h; }

        bool add(int id, int mod, int vk)
        {
            if (!open()) return false;
            remove(id);
            KeySym sym = (vk >= '0' && vk <= '9') ? (KeySym)(XK_0 + (vk - '0')) : (KeySym)(XK_a + (vk - 'A'));
            int kc = XKeysymToKeycode(dpy_, sym);
            if (kc == 0) return false;
            unsigned int mods = 0;
            if (mod & 0x2) mods |= ControlMask;
            if (mod & 0x1) mods |= Mod1Mask;
            if (mod & 0x4) mods |= ShiftMask;
            error_ = false;
            auto old = XSetErrorHandler(onError);
            for (unsigned int extra : {0u, (unsigned)LockMask, (unsigned)Mod2Mask, (unsigned)(LockMask | Mod2Mask)})
                XGrabKey(dpy_, kc, mods | extra, DefaultRootWindow(dpy_), 0, GrabModeAsync, GrabModeAsync);
            XSync(dpy_, 0);
            XSetErrorHandler(old);
            if (error_) {
                // another program owns the combination
                for (unsigned int extra : {0u, (unsigned)LockMask, (unsigned)Mod2Mask, (unsigned)(LockMask | Mod2Mask)})
                    XUngrabKey(dpy_, kc, mods | extra, DefaultRootWindow(dpy_));
                XSync(dpy_, 0);
                return false;
            }
            grabs_[id] = {mods, kc};
            return true;
        }

        void remove(int id)
        {
            auto it = grabs_.find(id);
            if (it == grabs_.end() || !dpy_) return;
            for (unsigned int extra : {0u, (unsigned)LockMask, (unsigned)Mod2Mask, (unsigned)(LockMask | Mod2Mask)})
                XUngrabKey(dpy_, it->second.keycode, it->second.mods | extra, DefaultRootWindow(dpy_));
            XSync(dpy_, 0);
            grabs_.erase(it);
        }

        bool supported() { return open(); }
        std::function<void(int)> handler;

    private:
        static bool error_;
        static int onError(Display *, XErrorEvent *) { error_ = true; return 0; }

        bool open()
        {
            if (dpy_) return true;
            if (tried_) return false;
            tried_ = true;
            // Under Wayland, X11 grabs only see keys typed into X11 windows, so they would not be global.
            if (isWayland()) { fx_hotkey_log("hotkeys: Wayland session - global hotkeys are not available"); return false; }
            dpy_ = XOpenDisplay(nullptr);
            if (!dpy_) { fx_hotkey_log("hotkeys: cannot open X display"); return false; }
            startTimer(50);
            return true;
        }

        void timerCallback() override
        {
            while (dpy_ && XPending(dpy_)) {
                XEvent ev;
                XNextEvent(dpy_, &ev);
                if (ev.type != FX_X11_KEYPRESS) continue;
                unsigned int m = ev.xkey.state & (ControlMask | Mod1Mask | ShiftMask);
                for (auto &g : grabs_)
                    if (g.second.keycode == (int)ev.xkey.keycode && g.second.mods == m && handler)
                        handler(g.first);
            }
        }

        Display *dpy_ = nullptr;
        bool tried_ = false;
        std::map<int, Grab> grabs_;
    };
    bool X11Hotkeys::error_ = false;
}

bool registerHotkey(int id, int mod, int vk)
{
    auto &h = X11Hotkeys::get();
    if (!h.handler)
        h.handler = fx_hotkey_default_handler();
    return h.add(id, mod, vk);
}
void unregisterHotkey(int id) { X11Hotkeys::get().remove(id); }
bool isSupported() { return X11Hotkeys::get().supported(); }
void setHandler(std::function<void(int)> handler) { X11Hotkeys::get().handler = std::move(handler); }
}
}
