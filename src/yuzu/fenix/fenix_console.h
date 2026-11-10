// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "common/common_types.h"

class QTimer;
class QVariantAnimation;

namespace Fenix {

/// A console-style home screen: the top bar with the profile and clock, one row of large game
/// tiles (most recent first, with an All Software tile at the end), a row of round buttons under
/// it and button hints at the bottom. Everything is painted here, and it is driven by the
/// keyboard, a controller (through the key events the game list posts) or the mouse.
class ConsoleView : public QWidget {
public:
    struct Tile {
        QString key;
        QString path;
        QString title;
        QPixmap icon;
        u64 program_id{};
        bool favorite{};
    };

    enum class Action {
        QuickSettings,
        AddGames,
        AllSoftware,
        Settings,
        DebugReport,
        Count,
    };

    explicit ConsoleView(QWidget* parent = nullptr);
    ~ConsoleView() override;

    /// The games to show, in order. The selection stays on the same game when it is still there.
    void SetTiles(std::vector<Tile> tiles);
    /// The key of the selected game (empty when a button or the All Software tile is selected).
    QString SelectedKey() const;

    std::function<void(const QString& path, u64 program_id)> on_launch;
    std::function<void(const QString& path, u64 program_id, const QPoint& global_pos)> on_menu;
    std::function<void(Action action)> on_action;
    std::function<void(u64 program_id)> on_favorite;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Metrics {
        qreal unit{1};
        qreal margin{};
        qreal tile{};
        qreal gap{};
        qreal row_top{};
        qreal title_y{};
        qreal button_size{};
        qreal button_gap{};
        qreal button_y{};
        qreal bottom_y{};
    };
    Metrics Measure() const;

    int TileCount() const; ///< Games plus the All Software tile
    QRectF TileRect(const Metrics& m, int index) const;
    QRectF ButtonRect(const Metrics& m, int index) const;
    int TileAt(const QPointF& pos) const;
    int ButtonAt(const QPointF& pos) const;

    void Select(int tile);
    void SelectButton(int button);
    void SetRow(int row);
    void Activate();
    void ActivateButton(int button);
    void OpenMenu(const QPoint& global_pos);
    void ScrollToSelected(bool animate);
    void UpdateTint();

    const QPixmap& Scaled(const Tile& tile, int size);
    void PaintTopBar(QPainter& painter, const Metrics& m);
    void PaintTiles(QPainter& painter, const Metrics& m);
    void PaintButtons(QPainter& painter, const Metrics& m);
    void PaintBottomBar(QPainter& painter, const Metrics& m);
    void PaintFocusFrame(QPainter& painter, const QRectF& rect, qreal radius, qreal unit);

    std::vector<Tile> tiles;
    int selected = 0;
    int previous = -1;
    int button = 0;
    int row = 0; ///< 0: tiles, 1: buttons
    int hover_button = -1;

    qreal scroll = 0;
    qreal scroll_target = 0;
    qreal grow = 1; ///< 0..1, how far the selected tile has grown in
    qreal pulse = 0;
    QColor tint{40, 40, 48};
    QColor tint_from{40, 40, 48};
    QColor tint_to{40, 40, 48};
    qreal tint_t = 1;

    QVariantAnimation* scroll_anim = nullptr;
    QVariantAnimation* grow_anim = nullptr;
    QVariantAnimation* tint_anim = nullptr;
    QTimer* pulse_timer = nullptr;
    QTimer* clock_timer = nullptr;
    QPixmap logo;
    QHash<QString, QPixmap> scaled_cache; ///< key + size
};

} // namespace Fenix
