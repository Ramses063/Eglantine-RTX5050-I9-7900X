// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "common/common_types.h"
#include "yuzu/fenix/fenix_library.h"

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QPropertyAnimation;
class QPushButton;
class QSlider;
class QStackedWidget;
class QStandardItemModel;
class QTimer;
class QVariantAnimation;
class GameList;
class IconSearch;

namespace PlayTime {
class PlayTimeManager;
}

namespace Fenix {

class CardDelegate;
class HeroBanner;

/// The Fenix home screen: a library of every scanned game as cards, with a Continue banner for
/// the last game, favorites, sorting (recent, name, play time, date added, own order), console
/// filters, search, and keyboard/controller navigation. It reads the emulator's game list model
/// (the scan itself, metadata and icons stay in GameList) and sends actions back through it.
class Home : public QWidget {
    Q_OBJECT

public:
    explicit Home(GameList* game_list, QStandardItemModel* source_model,
                  const PlayTime::PlayTimeManager& play_time_manager, QWidget* parent = nullptr);
    ~Home() override;

    /// Where controller/keyboard events for the home screen go.
    QWidget* KeyTarget() const;

    /// Rebuild the cards from the game list model soon (many model changes come in bursts).
    void ScheduleRebuild();

    /// Fenix's own entries for a game's menu (play, favorite, name, image, save backups, order).
    void AddGameActions(QMenu& menu, const QString& path, u64 program_id);

    /// Backs up a game's saves now (when backups are on); called before a game boots.
    static void BackupSavesBeforeBoot(u64 program_id);
    /// Backs up a game's saves now, whatever the automatic backup setting; says how it went.
    static QString BackupSavesNow(u64 program_id);

signals:
    void LaunchGame(const QString& path, u64 program_id);
    void ToggleFavoriteRequested(u64 program_id);
    void ContextMenuRequested(const QString& path, u64 program_id, const QPoint& global_pos);
    void AddFolderRequested();
    void OpenSettingsRequested();
    void DebugReportRequested();
    void FullscreenRequested();
    void RefreshRequested();
    void SaveConfigRequested();
    void OpenSaveFolderRequested(const QString& path, u64 program_id);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    struct Entry {
        QString key;
        QString path;
        QString title;
        QString original_title; ///< The game's own name, when renamed
        QPixmap icon;
        u64 program_id{};
        Console console{Console::Switch};
        u64 play_seconds{};
        qint64 last_played{};
        qint64 added{};
        bool favorite{};
    };

    void BuildUi();
    QWidget* BuildTopBar();
    QWidget* BuildHero();
    QWidget* BuildToolbar();
    QWidget* BuildEmptyPage();

    void Rebuild();
    void CollectEntries();
    void ApplyView();
    void UpdateHero();
    void UpdateChips();
    void SaveOrderFromCards();

    const Entry* CurrentEntry() const;
    const Entry* FindEntry(const QString& key) const;
    void LaunchCurrent();
    void ToggleFavoriteCurrent();
    void ToggleFavorite(const QString& key);
    void MoveChipPill(bool animate);
    void CycleSort(int step);
    void CycleFilter(int step);
    void MoveCurrent(int step);
    void MoveRow(int row, int target);
    void SetCardSize(int size);

    void AddSingleGame();
    /// Adds a folder chosen with a controller-friendly picker (- on a controller).
    void AddGamesWithPicker();
    void ManageFolders();
    void OpenQuickSettings();
    void ShowRestoreDialog(u64 program_id, const QString& title);
    void RenameGame(const QString& key, const QString& current_title,
                    const QString& original_title);
    void ChooseImage(const QString& key);
    void FindImageOnline(const QString& key, const QString& title, u64 program_id);
    QPixmap CustomImage(const QString& key);
    /// Looks the art of games that have none up online, one game at a time in the background.
    void QueueMissingArt();
    void FetchNextArt();

    GameList* game_list;
    QStandardItemModel* source;
    const PlayTime::PlayTimeManager& play_time;

    std::vector<Entry> entries;

    QStackedWidget* pages = nullptr; ///< library / empty
    QStackedWidget* grid_pages = nullptr; ///< grid / no results
    HeroBanner* hero = nullptr;
    QLabel* hero_icon = nullptr;
    QLabel* hero_title = nullptr;
    QLabel* hero_console = nullptr;
    QLabel* hero_time = nullptr;
    QLabel* hero_last = nullptr;
    QPushButton* hero_play = nullptr;
    QPushButton* hero_art = nullptr;
    QPushButton* hero_options = nullptr;
    QLineEdit* search = nullptr;
    std::array<QPushButton*, static_cast<size_t>(Filter::Count)> chips{};
    QWidget* chip_row = nullptr;
    QFrame* chip_pill = nullptr;
    QPropertyAnimation* chip_anim = nullptr;
    QVariantAnimation* star_anim = nullptr;
    QComboBox* sort_box = nullptr;
    QSlider* size_slider = nullptr;
    QLabel* count_label = nullptr;
    QLabel* no_results = nullptr;
    QListView* grid = nullptr;
    QStandardItemModel* cards = nullptr;
    CardDelegate* delegate = nullptr;
    QTimer* rebuild_timer = nullptr;

    QString hero_key;
    IconSearch* icon_search = nullptr;
    QString icon_search_key; ///< The game an online image search is for
    QHash<QString, QPixmap> custom_image_cache; ///< by file path
    struct ArtRequest {
        QString key;
        QString title;
        u64 program_id{};
    };
    std::vector<ArtRequest> art_queue;
    bool art_running = false;
    std::shared_ptr<std::atomic_bool> art_cancel = std::make_shared<std::atomic_bool>(false);
    bool applying_view = false;
    int drag_row = -1;
    QPoint drag_start;
    bool dragging = false;
};

} // namespace Fenix
