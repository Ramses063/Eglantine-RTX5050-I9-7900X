// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QKeyEvent>
#include <QObject>
#include <QTimer>

#include "common/input.h"
#include "common/settings_input.h"

namespace Core::HID {
using ButtonValues = std::array<Common::Input::ButtonStatus, Settings::NativeButton::NumButtons>;
using SticksValues = std::array<Common::Input::StickStatus, Settings::NativeAnalog::NumAnalogs>;
enum class ControllerTriggerType;
class EmulatedController;
class HIDCore;
} // namespace Core::HID

class ControllerNavigation : public QObject {
    Q_OBJECT

public:
    explicit ControllerNavigation(Core::HID::HIDCore& hid_core, QWidget* parent = nullptr);
    ~ControllerNavigation();

    /// Disables events from the emulated controller
    void UnloadController();

    /// A short buzz in the controller for menu feedback (0..1 strength).
    void Rumble(float strength);

signals:
    void TriggerKeyboardEvent(Qt::Key key);

private:
    void TriggerButton(Settings::NativeButton::Values native_button, Qt::Key key);
    void ControllerUpdateEvent(Core::HID::ControllerTriggerType type);

    void ControllerUpdateButton();

    void ControllerUpdateStick();

    Core::HID::ButtonValues button_values{};
    Core::HID::SticksValues stick_values{};

    int player1_callback_key{};
    int handheld_callback_key{};
    bool is_controller_set{};
    mutable std::mutex mutex;
    Core::HID::EmulatedController* player1_controller;
    Core::HID::EmulatedController* handheld_controller;
};
