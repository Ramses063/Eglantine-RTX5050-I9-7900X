// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QDialog>

#include "common/common_types.h"

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

    /// Ocarina of Time's frame rate (its built-in FPS unlock keeps real speed above 60): 60, 90,
    /// 120 or 144, 144 by default.
    [[nodiscard]] static u16 OcarinaFrameRate();

    /// A change made here only takes effect when the game starts again.
    [[nodiscard]] bool RestartNeeded() const {
        return restart_needed;
    }

    /// Opened over a running game (paused meanwhile): says which changes apply right away, and
    /// offers Close Game.
    void SetInGame(bool in_game, u64 game_program_id = 0);

    /// The player chose Close Game.
    [[nodiscard]] bool CloseGameRequested() const {
        return close_game;
    }

    /// Sends a controller key to the menu or dialog that is open, if any (true when one took
    /// it): menus, questions and Fenix dialogs all work with a controller.
    static bool RouteControllerKey(Qt::Key key);

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
    /// One tap for everything: Best, Frame Generation or 4K (2x resolution).
    void ApplyMode(int mode);
    /// The mode the settings match now, or -1 (custom).
    [[nodiscard]] static int CurrentMode();
    [[nodiscard]] static QStringList ModeNames();
    void UpdateModes();
    void ShowAdvanced(bool show);

    std::vector<Row> rows;
    QLabel* status = nullptr;
    bool restart_needed = false;
    bool close_game = false;
    QPushButton* close_game_button = nullptr;
    QPushButton* backup_button = nullptr;
    u64 program_id{};
    std::vector<QPushButton*> mode_buttons;
    QLabel* mode_note = nullptr;
    QPushButton* advanced_button = nullptr;
    QWidget* advanced_area = nullptr;
};

/// What to do when Fenix is closed during a game.
enum class LeaveChoice { KeepPlaying, CloseGame, QuitApp };

/// Asks: close the game (back to the library), quit Fenix, or keep playing. Controller friendly.
LeaveChoice AskLeaveGame(QWidget* parent, const QString& game_title);

/// A folder picker that works with a controller (big list, A opens, B goes up, Y picks the
/// folder shown). Returns the folder, or an empty string when cancelled.
QString PickFolder(QWidget* parent, const QString& start);

} // namespace Fenix
