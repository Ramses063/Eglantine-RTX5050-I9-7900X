// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QDialog>

class QButtonGroup;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace Fenix {

/// Big-button settings for the things people change most: a preset (Performance / Balanced /
/// Quality / Ultra, with one recommended for this PC), resolution, frame generation, the Switch 2
/// frame rate cap, VSync, docked or handheld, and NVIDIA DLSS. Works with a mouse, keyboard
/// or controller (arrows move between buttons, A/Enter picks, B/Escape closes). Changes apply at
/// once to the global settings.
class QuickSettings : public QDialog {
    Q_OBJECT

public:
    explicit QuickSettings(QWidget* parent = nullptr);

    /// Passes the choices read at startup or game start (speed-ups, shader loading, CPU cores)
    /// on to the emulator. Called once when Fenix starts and after each change.
    static void ApplyStartupOptions();

    /// Opened over a running game (paused meanwhile): says which changes apply right away.
    void SetInGame(bool in_game);

signals:
    void SettingsChanged();
    void OpenAllSettings();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    struct Row {
        QButtonGroup* group;
        std::function<int()> current;
    };

    void AddRow(QVBoxLayout* layout, const QString& title, const QString& hint,
                const QStringList& options, std::function<int()> current,
                std::function<void(int)> apply);
    void RefreshRows();
    void Changed();
    /// One tap for everything: Best, Smooth (frame generation), Sharp (2x) or Safe.
    void ApplyMode(int mode);
    /// The mode the settings match now, or -1 (custom).
    [[nodiscard]] static int CurrentMode();
    void UpdateModes();
    void ShowAdvanced(bool show);

    std::vector<Row> rows;
    QLabel* status = nullptr;
    std::vector<QPushButton*> mode_buttons;
    QLabel* mode_note = nullptr;
    QPushButton* advanced_button = nullptr;
    QWidget* advanced_area = nullptr;
};

} // namespace Fenix
