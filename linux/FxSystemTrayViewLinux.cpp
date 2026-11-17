/*
 * FxSystemTrayViewLinux.cpp - Linux implementation of FxSystemTrayView (tray icon, tray menu and
 * notifications), mirroring upstream GUI/FxSystemTrayView.cpp item by item.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include <JuceHeader.h>
#include "FxSystemTrayView.h"
#include "FxLinuxTray.h"
#include "FxLinuxFeatures.h"
#include "../Utils/SysInfo/SysInfo.h"

FxSystemTrayView::FxSystemTrayView()
{
    FxModel::getModel().addListener(this);
    custom_notification_ = true;

    tray_ = std::make_unique<FxLinuxTray>(
        [this](PopupMenu &m) { buildContextMenu(m); },
        []() {
            if (FxController::getInstance().isMainWindowVisible())
                FxController::getInstance().hideMainWindow();
            else
                FxController::getInstance().showMainWindow();
        });
}

FxSystemTrayView::~FxSystemTrayView()
{
    FxModel::getModel().removeListener(this);
    tray_.reset();
}

bool FxSystemTrayView::isTrayAvailable() const { return tray_ && tray_->isAvailable(); }

void FxSystemTrayView::modelChanged(FxModel::Event model_event)
{
    if (!FxController::getInstance().isNotificationsHidden() && model_event == FxModel::Event::Notification)
    {
        showNotification();
    }
    // preset list, selection, modified flag, outputs... are all shown in the tray menu
    if (model_event != FxModel::Event::Notification && tray_)
        tray_->refreshMenu();
}

void FxSystemTrayView::setStatus(bool power, bool processing)
{
    String param = power ? TRANS(L"on") : TRANS(L"off");
    String tip = String(TRANS("FxSound is %s.")).replace("%s", param);
    tip << "\n" << TRANS("Output: ") << String(FxModel::getModel().getSelectedOutput().deviceFriendlyName.c_str());
    if (tray_) tray_->setStatus(power, processing, tip);
}

Point<int> FxSystemTrayView::getSystemTrayWindowPosition(int width, int height)
{
    // Linux has no API for the tray icon position: put the window in the corner next to the panel.
    Point<int> pos = { 0, 0 };
    auto display = Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (display != nullptr)
    {
        auto area = display->userArea;
        auto total = display->totalArea;
        pos.x = area.getRight() - width - 10;
        bool panel_on_top = area.getY() > total.getY();
        bool panel_on_bottom = area.getBottom() < total.getBottom();
        if (panel_on_top || !panel_on_bottom)
            pos.y = area.getY() + 10;
        else
            pos.y = area.getBottom() - height - 10;
    }
    return pos;
}

void FxSystemTrayView::addIcon() {}

void FxSystemTrayView::buildContextMenu(PopupMenu& context_menu)
{
    PopupMenu preset_menu;
    PopupMenu theme_menu;
    PopupMenu donate_menu;

    auto id = PRESET_MENU_ID_START;
    auto count = FxModel::getModel().getPresetCount();
    auto preset_type = FxModel::PresetType::AppPreset;
    for (auto i = 0; i < count; i++)
    {
        auto preset = FxModel::getModel().getPreset(i);
        PopupMenu::Item menu_item(preset.modified ? preset.name + L" *" : preset.name);
        menu_item.setID(id);
        if (id - PRESET_MENU_ID_START == FxModel::getModel().getSelectedPreset())
            menu_item.setTicked(true);

        if (preset_type != preset.type)
        {
            preset_menu.addSeparator();
            preset_type = preset.type;
        }

        menu_item.setAction([preset = id]() { FxController::getInstance().setPreset(preset - PRESET_MENU_ID_START); });
        preset_menu.addItem(menu_item);
        id++;
    }

    auto settingsClicked = []() {
        FxSettingsDialog settings_dialog;
        settings_dialog.runModalLoop();
        FxController::getInstance().refreshOutputList();
    };

    theme_menu.addItem(TRANS("Dark"), true, FxTheme::getThemeMode() == FxThemeMode::Dark, []() { FxController::getInstance().setThemeMode(FxThemeMode::Dark); });
    theme_menu.addItem(TRANS("Light"), true, FxTheme::getThemeMode() == FxThemeMode::Light, []() { FxController::getInstance().setThemeMode(FxThemeMode::Light); });
    FxLinux::addDonateItems(donate_menu);

    PopupMenu::Item open(TRANS("Open"));
    open.setID(MENU_ID_OPEN);
    open.setAction([]() { FxController::getInstance().showMainWindow(); });

    PopupMenu::Item power;
    power.text = FxModel::getModel().getPowerState() ? String(TRANS("Turn Off")) : String(TRANS("Turn On"));
    power.setID(MENU_ID_POWER);
    power.setAction([]() { FxController::getInstance().setPowerState(!FxModel::getModel().getPowerState()); });
    power.setEnabled(!SysInfo::isRemoteSession());

    PopupMenu::Item settings(TRANS("Settings"));
    settings.setID(MENU_ID_SETTINGS);
    settings.setAction(settingsClicked);

    PopupMenu::Item exit(TRANS("Exit"));
    exit.setID(MENU_ID_EXIT);
    exit.setAction([]() { FxController::getInstance().exit(); });

    context_menu.addItem(open);
    context_menu.addItem(power);
    if (FxModel::getModel().getPowerState())
        context_menu.addSubMenu(TRANS("Preset Select"), preset_menu);
    addOutputDeviceMenu(&context_menu);
    context_menu.addItem(settings);
    context_menu.addSubMenu(TRANS("Theme"), theme_menu);
    auto *main_window = FxController::getInstance().getMainWindow();
    context_menu.addItem(TRANS("Always On Top"), true, main_window != nullptr && main_window->isAlwaysOnTop(), []() {
        auto& controller = FxController::getInstance();
        controller.setAlwaysOnTop(!controller.isAlwaysOnTop());
    });
    context_menu.addSubMenu(TRANS("Donate"), donate_menu);
    context_menu.addItem(exit);
}

void FxSystemTrayView::showContextMenu()
{
    FxController::getInstance().checkDeviceChanges();
    PopupMenu context_menu;
    buildContextMenu(context_menu);
    context_menu.showMenuAsync(PopupMenu::Options());
}

void FxSystemTrayView::addOutputDeviceMenu(PopupMenu* context_menu)
{
    PopupMenu output_menu;
    PopupMenu* menu;

    auto output_devices = FxModel::getModel().getOutputDevices();
    int num_outputs = (int)output_devices.size();
    if (num_outputs > 5)
    {
        menu = &output_menu;
    }
    else
    {
        menu = context_menu;
        menu->addSeparator();
        menu->addSectionHeader(TRANS("Playback Device Select"));
    }

    auto id = OUTPUT_MENU_ID_START;
    for (auto device : output_devices)
    {
        PopupMenu::Item menu_item(getTruncatedText(device.deviceFriendlyName.c_str(), 30));
        menu_item.setID(id);
        if (device.deviceNumChannel < 2)
            menu_item.setEnabled(false);
        if (id - OUTPUT_MENU_ID_START == FxModel::getModel().getSelectedOutputIndex())
            menu_item.setTicked(true);
        menu_item.setAction([output = id]() { FxController::getInstance().setOutput(output - OUTPUT_MENU_ID_START); });
        menu->addItem(menu_item);
        id++;
    }

    if (num_outputs > 5)
        context_menu->addSubMenu(TRANS("Playback Device Select"), output_menu);
    else
        menu->addSeparator();
}

void FxSystemTrayView::showNotification()
{
    String message;
    std::pair<String, String> link;
    FxModel::getModel().popMessage(message, link);

    if (message.isNotEmpty())
    {
        if (!tray_ || !tray_->isAvailable())
        {
            // no tray: the Windows text points at the tray icon, which the user cannot see
            if (message.startsWith(TRANS("FxSound in system tray")))
                message = TRANS("FxSound keeps running in the background.") + "\r\n" + TRANS("Open FxSound from your applications menu to show it again.");
        }
        notification_.setMessage(message, link);
        Point<int> pos = getSystemTrayWindowPosition(notification_.getWidth(), notification_.getHeight());
        notification_.setBounds(pos.x, pos.y, notification_.getWidth(), notification_.getHeight());
        notification_.showMessage();
    }
}

String FxSystemTrayView::getTruncatedText(const String& text, int max_length)
{
    if (text.length() <= max_length)
        return text;
    const String ellipsis = "...";
    int trunc_length = (text.length() - max_length) + ellipsis.length();
    return text.dropLastCharacters(trunc_length) + ellipsis;
}

// FxController::isTrayAvailable (declared in the Linux section of FxController.h)
bool FxController::isTrayAvailable()
{
    return system_tray_view_ != nullptr && system_tray_view_->isTrayAvailable();
}
