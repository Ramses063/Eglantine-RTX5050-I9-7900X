// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <climits>
#include <functional>
#include <thread>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSlider>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "common/fs/fs_util.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "frontend_common/play_time_manager.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"
#include "yuzu/fenix/fenix_home.h"
#include "yuzu/fenix/fenix_quick_settings.h"
#include "yuzu/fenix/fenix_saves.h"
#include "yuzu/fenix/fenix_theme.h"
#include "yuzu/game/game_list.h"

namespace Fenix {

namespace {

enum CardRole {
    KeyRole = Qt::UserRole + 100,
    PathRole,
    ProgramIdRole,
    ConsoleRole,
    PlaySecondsRole,
    FavoriteRole,
    LastPlayedRole,
};

constexpr int CardPadding = 10;
constexpr int CardMargin = 7;
constexpr int CardTextHeight = 58;

QString PlayTimeText(u64 seconds) {
    if (seconds == 0) {
        return QObject::tr("Not played yet");
    }
    const u64 hours = seconds / 3600;
    const u64 minutes = (seconds % 3600) / 60;
    if (hours == 0) {
        return QObject::tr("%1 min played").arg(std::max<u64>(minutes, 1));
    }
    return QObject::tr("%1 h %2 min played").arg(hours).arg(minutes);
}

QString AgoText(qint64 when) {
    if (when <= 0) {
        return {};
    }
    const qint64 seconds = QDateTime::currentSecsSinceEpoch() - when;
    if (seconds < 90) {
        return QObject::tr("just now");
    }
    if (seconds < 3600) {
        return QObject::tr("%1 minutes ago").arg(seconds / 60);
    }
    if (seconds < 86400) {
        return QObject::tr("%1 hours ago").arg(seconds / 3600);
    }
    if (seconds < 86400 * 2) {
        return QObject::tr("yesterday");
    }
    return QObject::tr("%1 days ago").arg(seconds / 86400);
}

std::filesystem::path SaveRoot() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::SaveDir);
}

std::filesystem::path BackupRoot() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir) / "fenix_backups";
}

/// A game's icon, or a red tile with its initial when it has none, cut to a rounded square.
QPixmap RoundedIcon(const QPixmap& source, const QString& title, int size, int radius) {
    const QString cache_key = QStringLiteral("fenix/%1/%2/%3")
                                  .arg(source.cacheKey())
                                  .arg(size)
                                  .arg(title.left(1));
    QPixmap cached;
    if (QPixmapCache::find(cache_key, &cached)) {
        return cached;
    }
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap result(QSize(size, size) * dpr);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
    painter.setClipPath(clip);
    // Placeholder underneath: shows through icons with transparency and stands in for none.
    QLinearGradient gradient(0, 0, size, size);
    gradient.setColorAt(0, Theme::RedDark);
    gradient.setColorAt(1, QColor(0x2a, 0x05, 0x0a));
    painter.fillRect(QRect(0, 0, size, size), gradient);
    QFont font = painter.font();
    font.setPixelSize(size / 2);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255, 60));
    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, title.left(1).toUpper());
    if (!source.isNull()) {
        painter.drawPixmap(QRect(0, 0, size, size), source);
    }
    painter.end();
    QPixmapCache::insert(cache_key, result);
    return result;
}

/// Rounded pixmap for QLabel use (the Continue banner).
QPixmap LabelIcon(const QPixmap& icon, const QString& title, int size) {
    return RoundedIcon(icon, title, size, 18);
}

} // namespace

/// Draws one game card: icon, console badge, favorite star, title, play time.
class CardDelegate final : public QStyledItemDelegate {
public:
    explicit CardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    std::function<void(const QModelIndex&)> on_favorite_clicked;

    void SetIconSize(int size) {
        icon_size = size;
    }
    int IconSize() const {
        return icon_size;
    }
    QSize CardSize() const {
        return {icon_size + 2 * (CardPadding + CardMargin),
                icon_size + CardPadding + CardTextHeight + 2 * CardMargin};
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return CardSize();
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const QRectF card = QRectF(option.rect).adjusted(CardMargin, CardMargin, -CardMargin,
                                                         -CardMargin);

        // Glow for the selected card (the controller cursor), then the card itself.
        if (selected) {
            for (int i = 4; i >= 1; --i) {
                QColor glow = Theme::Red;
                glow.setAlpha(18 * (5 - i));
                painter->setPen(Qt::NoPen);
                painter->setBrush(glow);
                painter->drawRoundedRect(card.adjusted(-i * 1.5, -i * 1.5, i * 1.5, i * 1.5),
                                         18 + i, 18 + i);
            }
        }
        painter->setPen(selected ? QPen(Theme::RedBright, 2.5) : QPen(Theme::Border, 1));
        painter->setBrush(hovered || selected ? Theme::SurfaceHover : Theme::Surface);
        painter->drawRoundedRect(card, 16, 16);

        // Icon.
        const QRect icon_rect(static_cast<int>(card.x()) + CardPadding,
                              static_cast<int>(card.y()) + CardPadding, icon_size, icon_size);
        const QString title = index.data(Qt::DisplayRole).toString();
        const QPixmap icon = RoundedIcon(index.data(Qt::DecorationRole).value<QPixmap>(), title,
                                         icon_size, 12);
        painter->drawPixmap(icon_rect.topLeft(), icon);

        // Console badge.
        const bool switch2 = index.data(ConsoleRole).toInt() == static_cast<int>(Console::Switch2);
        QFont badge_font = option.font;
        badge_font.setPixelSize(10);
        badge_font.setBold(true);
        badge_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        painter->setFont(badge_font);
        const QString badge_text = switch2 ? QStringLiteral("SWITCH 2") : QStringLiteral("SWITCH");
        const int badge_width = QFontMetrics(badge_font).horizontalAdvance(badge_text) + 14;
        const QRectF badge(icon_rect.x() + 8, icon_rect.y() + 8, badge_width, 20);
        painter->setPen(Qt::NoPen);
        painter->setBrush(switch2 ? Theme::Red : QColor(20, 20, 26, 220));
        painter->drawRoundedRect(badge, 10, 10);
        painter->setPen(Qt::white);
        painter->drawText(badge, Qt::AlignCenter, badge_text);

        // Favorite star (always shown when set; on hover/selection otherwise).
        const bool favorite = index.data(FavoriteRole).toBool();
        if (favorite || hovered || selected) {
            const QRectF star = StarRect(icon_rect);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(12, 12, 16, 190));
            painter->drawEllipse(star);
            QFont star_font = option.font;
            star_font.setPixelSize(17);
            painter->setFont(star_font);
            painter->setPen(favorite ? Theme::Gold : QColor(255, 255, 255, 170));
            painter->drawText(star, Qt::AlignCenter,
                              favorite ? QStringLiteral("★") : QStringLiteral("☆"));
        }

        // Title (up to two lines) and play time.
        const QRect text_rect(icon_rect.x(), icon_rect.bottom() + 8, icon_size, CardTextHeight - 8);
        QFont title_font = option.font;
        title_font.setPixelSize(13);
        title_font.setBold(true);
        painter->setFont(title_font);
        painter->setPen(Theme::Text);
        const QFontMetrics title_metrics(title_font);
        const QRect title_rect(text_rect.x(), text_rect.y(), text_rect.width(),
                               title_metrics.lineSpacing() * 2);
        const QString shown_title =
            title_metrics.horizontalAdvance(title) <= text_rect.width() * 2 - 20
                ? title
                : title_metrics.elidedText(title, Qt::ElideRight, text_rect.width() * 2 - 30);
        painter->drawText(title_rect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                          shown_title);
        QFont meta_font = option.font;
        meta_font.setPixelSize(11);
        painter->setFont(meta_font);
        painter->setPen(Theme::TextMuted);
        painter->drawText(QRect(text_rect.x(), text_rect.bottom() - 14, text_rect.width(), 16),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          PlayTimeText(index.data(PlaySecondsRole).toULongLong()));
        painter->restore();
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            const QRectF card = QRectF(option.rect).adjusted(CardMargin, CardMargin, -CardMargin,
                                                             -CardMargin);
            const QRect icon_rect(static_cast<int>(card.x()) + CardPadding,
                                  static_cast<int>(card.y()) + CardPadding, icon_size, icon_size);
            if (mouse->button() == Qt::LeftButton &&
                StarRect(icon_rect).contains(mouse->position()) && on_favorite_clicked) {
                on_favorite_clicked(index);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

private:
    static QRectF StarRect(const QRect& icon_rect) {
        return QRectF(icon_rect.right() - 36, icon_rect.y() + 6, 30, 30);
    }

    int icon_size{176};
};


// ---------------------------------------------------------------------------------------------
// Home

Home::Home(GameList* game_list_, QStandardItemModel* source_model,
           const PlayTime::PlayTimeManager& play_time_manager, QWidget* parent)
    : QWidget(parent), game_list{game_list_}, source{source_model}, play_time{play_time_manager} {
    setObjectName(QStringLiteral("FenixRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(Theme::StyleSheet());

    rebuild_timer = new QTimer(this);
    rebuild_timer->setSingleShot(true);
    rebuild_timer->setInterval(150);
    connect(rebuild_timer, &QTimer::timeout, this, &Home::Rebuild);

    BuildUi();

    // The scan fills the model in bursts; rebuild the cards once each burst settles.
    connect(source, &QAbstractItemModel::rowsInserted, this, &Home::ScheduleRebuild);
    connect(source, &QAbstractItemModel::rowsRemoved, this, &Home::ScheduleRebuild);
    connect(source, &QAbstractItemModel::modelReset, this, &Home::ScheduleRebuild);
    connect(source, &QAbstractItemModel::dataChanged, this, &Home::ScheduleRebuild);
    ScheduleRebuild();
}

Home::~Home() {
    Library::Get().Save();
}

QWidget* Home::KeyTarget() const {
    if (search->hasFocus()) {
        return search;
    }
    return grid;
}

void Home::ScheduleRebuild() {
    rebuild_timer->start();
}

void Home::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    ScheduleRebuild();
    QTimer::singleShot(0, this, [this] {
        if (isVisible() && !search->hasFocus()) {
            grid->setFocus();
        }
    });
}

// --- layout ----------------------------------------------------------------------------------

void Home::BuildUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);
    root_layout->addWidget(BuildTopBar());

    pages = new QStackedWidget(this);
    root_layout->addWidget(pages, 1);

    auto* library_page = new QWidget(pages);
    library_page->setObjectName(QStringLiteral("FenixPage"));
    auto* library_layout = new QVBoxLayout(library_page);
    library_layout->setContentsMargins(28, 22, 20, 12);
    library_layout->setSpacing(16);
    library_layout->addWidget(BuildHero());
    library_layout->addWidget(BuildToolbar());

    grid_pages = new QStackedWidget(library_page);
    cards = new QStandardItemModel(this);
    grid = new QListView(grid_pages);
    grid->setModel(cards);
    delegate = new CardDelegate(grid);
    delegate->SetIconSize(Library::Get().CardSize());
    delegate->on_favorite_clicked = [this](const QModelIndex& index) {
        emit ToggleFavoriteRequested(index.data(ProgramIdRole).toULongLong());
    };
    grid->setItemDelegate(delegate);
    grid->setViewMode(QListView::IconMode);
    grid->setResizeMode(QListView::Adjust);
    grid->setMovement(QListView::Static);
    grid->setFlow(QListView::LeftToRight);
    grid->setWrapping(true);
    grid->setUniformItemSizes(true);
    grid->setGridSize(delegate->CardSize());
    grid->setSpacing(0);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    grid->verticalScrollBar()->setSingleStep(36);
    grid->setMouseTracking(true);
    grid->setContextMenuPolicy(Qt::CustomContextMenu);
    grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Cards are dragged to a new place by hand (eventFilter): QListView's own drag and drop in
    // icon mode moves items to free positions instead of reordering the model.
    grid->setDragEnabled(false);
    grid->installEventFilter(this);
    grid->viewport()->installEventFilter(this);
    connect(grid, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (index.isValid()) {
            emit LaunchGame(index.data(PathRole).toString(), index.data(ProgramIdRole).toULongLong());
        }
    });
    connect(grid, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = grid->indexAt(pos);
        if (!index.isValid()) {
            return;
        }
        grid->setCurrentIndex(index);
        emit ContextMenuRequested(index.data(PathRole).toString(),
                                  index.data(ProgramIdRole).toULongLong(),
                                  grid->viewport()->mapToGlobal(pos));
    });
    grid_pages->addWidget(grid);

    no_results = new QLabel(tr("No games match.\nTry another filter or search."), grid_pages);
    no_results->setObjectName(QStringLiteral("FenixMuted"));
    no_results->setAlignment(Qt::AlignCenter);
    QFont no_results_font = no_results->font();
    no_results_font.setPixelSize(16);
    no_results->setFont(no_results_font);
    grid_pages->addWidget(no_results);
    library_layout->addWidget(grid_pages, 1);

    auto* hints = new QLabel(
        tr("A / Enter: play    X / F: favorite    Y / S: sort    L R / PgUp PgDn: filter    "
           "+ / Menu: quick settings    Drag cards to reorder"),
        library_page);
    hints->setObjectName(QStringLiteral("FenixMuted"));
    QFont hints_font = hints->font();
    hints_font.setPixelSize(11);
    hints->setFont(hints_font);
    library_layout->addWidget(hints);

    pages->addWidget(library_page);
    pages->addWidget(BuildEmptyPage());
}

QWidget* Home::BuildTopBar() {
    auto* bar = new QFrame(this);
    bar->setObjectName(QStringLiteral("FenixTopBar"));
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(24, 12, 20, 12);
    layout->setSpacing(10);

    auto* logo = new QLabel(bar);
    logo->setPixmap(QPixmap(QStringLiteral(":/img/fenix.png"))
                        .scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo);

    auto* brand_box = new QVBoxLayout;
    brand_box->setSpacing(0);
    auto* brand = new QLabel(QStringLiteral("FENIX"), bar);
    brand->setObjectName(QStringLiteral("FenixBrand"));
    auto* subtitle = new QLabel(tr("Nintendo Switch games"), bar);
    subtitle->setObjectName(QStringLiteral("FenixSubtitle"));
    brand_box->addWidget(brand);
    brand_box->addWidget(subtitle);
    layout->addLayout(brand_box);
    layout->addSpacing(24);

    search = new QLineEdit(bar);
    search->setPlaceholderText(tr("Search games  (Ctrl+F)"));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(260);
    search->setMaximumWidth(420);
    search->installEventFilter(this);
    connect(search, &QLineEdit::textChanged, this, [this] { ApplyView(); });
    layout->addWidget(search, 1);
    layout->addStretch();

    const auto make_button = [bar, layout](const QString& text, const QString& tip) {
        auto* button = new QPushButton(text, bar);
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        layout->addWidget(button);
        return button;
    };
    connect(make_button(tr("+ Add Game"), tr("Add a single game file")), &QPushButton::clicked,
            this, &Home::AddSingleGame);
    connect(make_button(tr("Folders"), tr("Choose the folders Fenix scans for games")),
            &QPushButton::clicked, this, &Home::ManageFolders);
    connect(make_button(tr("⟳ Refresh"), tr("Scan the game folders again (F5)")),
            &QPushButton::clicked, this, &Home::RefreshRequested);
    connect(make_button(tr("⚙ Quick Settings"), tr("Graphics presets, resolution, FPS")),
            &QPushButton::clicked, this, &Home::OpenQuickSettings);
    connect(make_button(tr("All Settings"), tr("Every emulator setting")), &QPushButton::clicked,
            this, &Home::OpenSettingsRequested);
    connect(make_button(tr("Debug Report"),
                        tr("Copy a report for troubleshooting to the clipboard (Ctrl+Shift+D)")),
            &QPushButton::clicked, this, &Home::DebugReportRequested);
    return bar;
}

QWidget* Home::BuildHero() {
    hero = new QFrame(this);
    hero->setObjectName(QStringLiteral("FenixHero"));
    auto* layout = new QHBoxLayout(hero);
    layout->setContentsMargins(20, 18, 24, 18);
    layout->setSpacing(22);

    hero_icon = new QLabel(hero);
    hero_icon->setFixedSize(132, 132);
    layout->addWidget(hero_icon);

    auto* text = new QVBoxLayout;
    text->setSpacing(6);
    auto* kicker = new QLabel(tr("CONTINUE PLAYING"), hero);
    kicker->setObjectName(QStringLiteral("FenixHeroKicker"));
    hero_title = new QLabel(hero);
    hero_title->setObjectName(QStringLiteral("FenixHeroTitle"));
    hero_title->setWordWrap(true);
    hero_meta = new QLabel(hero);
    hero_meta->setObjectName(QStringLiteral("FenixMuted"));
    hero_play = new QPushButton(tr("▶  Continue"), hero);
    hero_play->setObjectName(QStringLiteral("FenixPrimary"));
    hero_play->setCursor(Qt::PointingHandCursor);
    hero_play->setFixedWidth(180);
    connect(hero_play, &QPushButton::clicked, this, [this] {
        if (const Entry* entry = FindEntry(hero_key)) {
            emit LaunchGame(entry->path, entry->program_id);
        }
    });
    text->addStretch();
    text->addWidget(kicker);
    text->addWidget(hero_title);
    text->addWidget(hero_meta);
    text->addSpacing(6);
    text->addWidget(hero_play);
    text->addStretch();
    layout->addLayout(text, 1);
    hero->setVisible(false);
    return hero;
}

QWidget* Home::BuildToolbar() {
    auto* toolbar = new QWidget(this);
    auto* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* section = new QLabel(tr("Library"), toolbar);
    section->setObjectName(QStringLiteral("FenixSection"));
    layout->addWidget(section);
    count_label = new QLabel(toolbar);
    count_label->setObjectName(QStringLiteral("FenixMuted"));
    layout->addWidget(count_label);
    layout->addSpacing(14);

    const std::array<QString, static_cast<size_t>(Filter::Count)> chip_names{
        tr("All"), tr("★ Favorites"), tr("Switch 2"), tr("Switch")};
    for (size_t i = 0; i < chips.size(); ++i) {
        auto* chip = new QPushButton(chip_names[i], toolbar);
        chip->setObjectName(QStringLiteral("FenixChip"));
        chip->setCheckable(true);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFocusPolicy(Qt::NoFocus);
        connect(chip, &QPushButton::clicked, this, [this, i] {
            Library::Get().SetFilter(static_cast<Filter>(i));
            UpdateChips();
            ApplyView();
        });
        chips[i] = chip;
        layout->addWidget(chip);
    }
    layout->addStretch();

    auto* sort_label = new QLabel(tr("Sort"), toolbar);
    sort_label->setObjectName(QStringLiteral("FenixMuted"));
    layout->addWidget(sort_label);
    sort_box = new QComboBox(toolbar);
    sort_box->addItems({tr("Recently played"), tr("Name"), tr("Play time"), tr("Date added"),
                        tr("My order")});
    sort_box->setCurrentIndex(static_cast<int>(Library::Get().Sort()));
    sort_box->setFocusPolicy(Qt::NoFocus);
    connect(sort_box, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (applying_view) {
            return;
        }
        Library::Get().SetSort(static_cast<SortMode>(index));
        ApplyView();
    });
    layout->addWidget(sort_box);
    layout->addSpacing(10);

    auto* size_label = new QLabel(tr("Size"), toolbar);
    size_label->setObjectName(QStringLiteral("FenixMuted"));
    layout->addWidget(size_label);
    size_slider = new QSlider(Qt::Horizontal, toolbar);
    size_slider->setRange(120, 320);
    size_slider->setValue(Library::Get().CardSize());
    size_slider->setFixedWidth(110);
    size_slider->setFocusPolicy(Qt::NoFocus);
    connect(size_slider, &QSlider::valueChanged, this, &Home::SetCardSize);
    layout->addWidget(size_slider);
    UpdateChips();
    return toolbar;
}

QWidget* Home::BuildEmptyPage() {
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("FenixPage"));
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(14);
    auto* logo = new QLabel(page);
    logo->setPixmap(QPixmap(QStringLiteral(":/img/fenix.png"))
                        .scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    auto* title = new QLabel(tr("Let's build your library"), page);
    title->setObjectName(QStringLiteral("FenixEmptyTitle"));
    title->setAlignment(Qt::AlignCenter);
    auto* text = new QLabel(tr("Add the folders where you keep your Nintendo Switch and Switch 2 "
                               "games.\nFenix finds every game in them, with its name and art."),
                            page);
    text->setObjectName(QStringLiteral("FenixMuted"));
    text->setAlignment(Qt::AlignCenter);
    auto* buttons = new QHBoxLayout;
    buttons->setAlignment(Qt::AlignCenter);
    auto* add_folder = new QPushButton(tr("Add a game folder"), page);
    add_folder->setObjectName(QStringLiteral("FenixPrimary"));
    add_folder->setCursor(Qt::PointingHandCursor);
    connect(add_folder, &QPushButton::clicked, this, &Home::AddFolderRequested);
    auto* add_game = new QPushButton(tr("Add a single game"), page);
    add_game->setCursor(Qt::PointingHandCursor);
    connect(add_game, &QPushButton::clicked, this, &Home::AddSingleGame);
    buttons->addWidget(add_folder);
    buttons->addWidget(add_game);
    layout->addWidget(logo);
    layout->addWidget(title);
    layout->addWidget(text);
    layout->addSpacing(8);
    layout->addLayout(buttons);
    return page;
}

// --- data ------------------------------------------------------------------------------------

void Home::CollectEntries() {
    entries.clear();
    QSet<QString> seen_paths;
    auto& library = Library::Get();
    // Walk the whole tree: folder rows hold games, the favorites row holds copies of them.
    std::function<void(QStandardItem*)> walk = [&](QStandardItem* parent) {
        for (int row = 0; row < parent->rowCount(); ++row) {
            QStandardItem* item = parent->child(row, 0);
            if (item == nullptr) {
                continue;
            }
            const auto type = item->data(GameListItem::TypeRole).value<GameListItemType>();
            if (type == GameListItemType::Game) {
                const QString path = item->data(GameListItemPath::FullPathRole).toString();
                if (path.isEmpty() || seen_paths.contains(path)) {
                    continue;
                }
                seen_paths.insert(path);
                Entry entry;
                entry.path = path;
                entry.program_id = item->data(GameListItemPath::ProgramIdRole).toULongLong();
                entry.title = item->data(GameListItemPath::TitleRole).toString();
                if (entry.title.isEmpty()) {
                    entry.title = QFileInfo(path).completeBaseName();
                }
                entry.icon = item->data(GameListItemPath::FullIconRole).value<QPixmap>();
                if (entry.icon.isNull()) {
                    entry.icon = item->data(Qt::DecorationRole).value<QPixmap>();
                }
                entry.console = ConsoleOf(entry.program_id);
                entry.key = Library::KeyOf(path, entry.program_id);
                entry.play_seconds = entry.program_id != 0 ? play_time.GetPlayTime(entry.program_id)
                                                           : 0;
                entry.last_played = library.LastPlayed(entry.key);
                entry.added = library.AddedAt(entry.key);
                entry.favorite = entry.program_id != 0 &&
                                 UISettings::values.favorited_ids.contains(entry.program_id);
                entries.push_back(std::move(entry));
            } else if (item->hasChildren()) {
                walk(item);
            }
        }
    };
    walk(source->invisibleRootItem());
    library.Save();
}

void Home::Rebuild() {
    CollectEntries();
    pages->setCurrentIndex(entries.empty() ? 1 : 0);
    UpdateHero();
    ApplyView();
}

void Home::ApplyView() {
    auto& library = Library::Get();
    const Filter filter = library.CurrentFilter();
    const SortMode sort = library.Sort();
    const QString text = search->text().trimmed();

    std::vector<const Entry*> shown;
    for (const auto& entry : entries) {
        if (filter == Filter::Favorites && !entry.favorite) {
            continue;
        }
        if (filter == Filter::Switch2 && entry.console != Console::Switch2) {
            continue;
        }
        if (filter == Filter::Switch1 && entry.console != Console::Switch) {
            continue;
        }
        if (!text.isEmpty() && !entry.title.contains(text, Qt::CaseInsensitive) &&
            !entry.key.contains(text, Qt::CaseInsensitive)) {
            continue;
        }
        shown.push_back(&entry);
    }

    const auto by_name = [](const Entry* a, const Entry* b) {
        return QString::localeAwareCompare(a->title, b->title) < 0;
    };
    switch (sort) {
    case SortMode::Recent:
        std::stable_sort(shown.begin(), shown.end(), [&](const Entry* a, const Entry* b) {
            if (a->last_played != b->last_played) {
                return a->last_played > b->last_played;
            }
            if (a->added != b->added) {
                return a->added > b->added;
            }
            return by_name(a, b);
        });
        break;
    case SortMode::Name:
        std::stable_sort(shown.begin(), shown.end(), by_name);
        break;
    case SortMode::PlayTime:
        std::stable_sort(shown.begin(), shown.end(), [&](const Entry* a, const Entry* b) {
            if (a->play_seconds != b->play_seconds) {
                return a->play_seconds > b->play_seconds;
            }
            return by_name(a, b);
        });
        break;
    case SortMode::Added:
        std::stable_sort(shown.begin(), shown.end(), [&](const Entry* a, const Entry* b) {
            if (a->added != b->added) {
                return a->added > b->added;
            }
            return by_name(a, b);
        });
        break;
    case SortMode::Custom:
    default: {
        const QStringList order = library.CustomOrder();
        QHash<QString, int> position;
        for (int i = 0; i < order.size(); ++i) {
            position.insert(order[i], i);
        }
        std::stable_sort(shown.begin(), shown.end(), [&](const Entry* a, const Entry* b) {
            const int pa = position.value(a->key, INT_MAX);
            const int pb = position.value(b->key, INT_MAX);
            if (pa != pb) {
                return pa < pb;
            }
            return by_name(a, b);
        });
        break;
    }
    }

    // Keep the selected game selected across rebuilds.
    const QString selected_key = grid->currentIndex().data(KeyRole).toString();

    applying_view = true;
    cards->clear();
    int select_row = 0;
    for (const Entry* entry : shown) {
        auto* item = new QStandardItem(entry->title);
        item->setData(entry->icon, Qt::DecorationRole);
        item->setData(entry->key, KeyRole);
        item->setData(entry->path, PathRole);
        item->setData(QVariant::fromValue<qulonglong>(entry->program_id), ProgramIdRole);
        item->setData(static_cast<int>(entry->console), ConsoleRole);
        item->setData(QVariant::fromValue<qulonglong>(entry->play_seconds), PlaySecondsRole);
        item->setData(entry->favorite, FavoriteRole);
        item->setData(entry->last_played, LastPlayedRole);
        item->setToolTip(QStringLiteral("%1\n%2 • %3%4")
                             .arg(entry->title, ConsoleName(entry->console),
                                  PlayTimeText(entry->play_seconds),
                                  entry->last_played > 0
                                      ? tr("\nLast played %1").arg(AgoText(entry->last_played))
                                      : QString{}));
        // Draggable, never a drop target itself (a drop on a card would nest it).
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        if (entry->key == selected_key) {
            select_row = cards->rowCount();
        }
        cards->appendRow(item);
    }
    applying_view = false;

    grid_pages->setCurrentIndex(shown.empty() ? 1 : 0);
    count_label->setText(entries.size() == shown.size()
                             ? tr("%n game(s)", nullptr, static_cast<int>(entries.size()))
                             : tr("%1 of %2").arg(shown.size()).arg(entries.size()));
    hero->setVisible(!hero_key.isEmpty() && text.isEmpty() && filter == Filter::All);
    sort_box->blockSignals(true);
    sort_box->setCurrentIndex(static_cast<int>(sort));
    sort_box->blockSignals(false);
    if (cards->rowCount() > 0) {
        grid->setCurrentIndex(cards->index(std::min(select_row, cards->rowCount() - 1), 0));
    }
}

void Home::UpdateHero() {
    const auto& library = Library::Get();
    const Entry* last = nullptr;
    if (!library.LastGamePath().isEmpty()) {
        const QString key = Library::KeyOf(library.LastGamePath(), library.LastGameId());
        last = FindEntry(key);
    }
    if (last == nullptr) {
        // Otherwise the most recently played game the library knows.
        for (const auto& entry : entries) {
            if (entry.last_played > 0 && (last == nullptr || entry.last_played > last->last_played)) {
                last = &entry;
            }
        }
    }
    hero_key = last != nullptr ? last->key : QString{};
    if (last == nullptr) {
        hero->setVisible(false);
        return;
    }
    hero_icon->setPixmap(LabelIcon(last->icon, last->title, 132));
    hero_title->setText(last->title);
    QStringList meta{ConsoleName(last->console), PlayTimeText(last->play_seconds)};
    if (last->last_played > 0) {
        meta << tr("Last played %1").arg(AgoText(last->last_played));
    }
    hero_meta->setText(meta.join(QStringLiteral("  •  ")));
}

void Home::UpdateChips() {
    const auto current = static_cast<size_t>(Library::Get().CurrentFilter());
    for (size_t i = 0; i < chips.size(); ++i) {
        chips[i]->setChecked(i == current);
    }
}

void Home::SaveOrderFromCards() {
    // The visible cards in their new order, then everything else in its previous order.
    QStringList order;
    QSet<QString> in_order;
    for (int row = 0; row < cards->rowCount(); ++row) {
        const QString key = cards->index(row, 0).data(KeyRole).toString();
        if (!key.isEmpty() && !in_order.contains(key)) {
            order << key;
            in_order.insert(key);
        }
    }
    for (const QString& key : Library::Get().CustomOrder()) {
        if (!in_order.contains(key)) {
            order << key;
            in_order.insert(key);
        }
    }
    for (const auto& entry : entries) {
        if (!in_order.contains(entry.key)) {
            order << entry.key;
            in_order.insert(entry.key);
        }
    }
    Library::Get().SetCustomOrder(order);
    if (Library::Get().Sort() != SortMode::Custom) {
        Library::Get().SetSort(SortMode::Custom);
        sort_box->blockSignals(true);
        sort_box->setCurrentIndex(static_cast<int>(SortMode::Custom));
        sort_box->blockSignals(false);
    }
}

const Home::Entry* Home::CurrentEntry() const {
    return FindEntry(grid->currentIndex().data(KeyRole).toString());
}

const Home::Entry* Home::FindEntry(const QString& key) const {
    if (key.isEmpty()) {
        return nullptr;
    }
    for (const auto& entry : entries) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

// --- actions ---------------------------------------------------------------------------------

void Home::LaunchCurrent() {
    if (const Entry* entry = CurrentEntry()) {
        emit LaunchGame(entry->path, entry->program_id);
    }
}

void Home::ToggleFavoriteCurrent() {
    if (const Entry* entry = CurrentEntry(); entry != nullptr && entry->program_id != 0) {
        emit ToggleFavoriteRequested(entry->program_id);
    }
}

void Home::CycleSort(int step) {
    const int count = static_cast<int>(SortMode::Count);
    const int next = (static_cast<int>(Library::Get().Sort()) + step + count) % count;
    Library::Get().SetSort(static_cast<SortMode>(next));
    ApplyView();
}

void Home::CycleFilter(int step) {
    const int count = static_cast<int>(Filter::Count);
    const int next = (static_cast<int>(Library::Get().CurrentFilter()) + step + count) % count;
    Library::Get().SetFilter(static_cast<Filter>(next));
    UpdateChips();
    ApplyView();
}

void Home::MoveCurrent(int step) {
    MoveRow(grid->currentIndex().row(), grid->currentIndex().row() + step);
}

void Home::MoveRow(int row, int target) {
    if (row < 0 || target < 0 || row >= cards->rowCount() || target >= cards->rowCount() ||
        row == target) {
        return;
    }
    applying_view = true;
    const QList<QStandardItem*> taken = cards->takeRow(row);
    cards->insertRow(target, taken);
    applying_view = false;
    grid->setCurrentIndex(cards->index(target, 0));
    SaveOrderFromCards();
}

void Home::SetCardSize(int size) {
    Library::Get().SetCardSize(size);
    delegate->SetIconSize(Library::Get().CardSize());
    grid->setGridSize(delegate->CardSize());
    grid->doItemsLayout();
}

void Home::AddSingleGame() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Add a game"), QString{},
        tr("Switch games (*.nup *.nsp *.xci *.nca *.nso *.nro *.kip *.nsz *.xcz main);;"
           "All files (*)"));
    if (file.isEmpty()) {
        return;
    }
    // Games come from scanned folders: add this game's folder (without its subfolders).
    QString folder = QFileInfo(file).absolutePath();
    if (QFileInfo(file).fileName() == QStringLiteral("main")) {
        // A dump's exefs/main: scan the dump's parent so the dump is found as one game.
        QDir dir{folder};
        dir.cdUp();
        dir.cdUp();
        folder = dir.absolutePath();
    }
    UISettings::GameDir game_dir{QDir::toNativeSeparators(folder).toStdString(), false, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
        emit SaveConfigRequested();
    }
    emit RefreshRequested();
    QMessageBox::information(this, tr("Game added"),
                             tr("Added the folder of this game to your library:\n%1\n\n"
                                "Other games in the same folder show up too.")
                                 .arg(QDir::toNativeSeparators(folder)));
}

void Home::ManageFolders() {
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("FenixRoot"));
    dialog.setStyleSheet(Theme::StyleSheet() +
                         QStringLiteral("QDialog#FenixRoot { background: #15151b; }"));
    dialog.setWindowTitle(tr("Game folders"));
    dialog.resize(640, 420);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    auto* title = new QLabel(tr("Game folders"), &dialog);
    title->setObjectName(QStringLiteral("FenixSection"));
    auto* hint = new QLabel(tr("Fenix scans these folders for games. Check a folder to scan its "
                               "subfolders too."),
                            &dialog);
    hint->setObjectName(QStringLiteral("FenixMuted"));
    hint->setWordWrap(true);
    auto* list = new QListWidget(&dialog);
    const auto special = [](const std::string& path) {
        return path == "SDMC" || path == "UserNAND" || path == "SysNAND";
    };
    const auto fill = [&] {
        list->clear();
        for (const auto& dir : UISettings::values.game_dirs) {
            if (special(dir.path)) {
                continue;
            }
            auto* item = new QListWidgetItem(QString::fromStdString(dir.path), list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(dir.deep_scan ? Qt::Checked : Qt::Unchecked);
        }
    };
    fill();
    bool changed = false;
    connect(list, &QListWidget::itemChanged, &dialog, [&](QListWidgetItem* item) {
        for (auto& dir : UISettings::values.game_dirs) {
            if (QString::fromStdString(dir.path) == item->text()) {
                dir.deep_scan = item->checkState() == Qt::Checked;
                changed = true;
            }
        }
    });
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(tr("+ Add Folder"), &dialog);
    add->setObjectName(QStringLiteral("FenixPrimary"));
    auto* remove = new QPushButton(tr("Remove"), &dialog);
    auto* open = new QPushButton(tr("Open"), &dialog);
    auto* close = new QPushButton(tr("Done"), &dialog);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addWidget(open);
    buttons->addStretch();
    buttons->addWidget(close);
    connect(add, &QPushButton::clicked, &dialog, [&] {
        const QString dir_path =
            QFileDialog::getExistingDirectory(&dialog, tr("Select a game folder"));
        if (dir_path.isEmpty()) {
            return;
        }
        UISettings::GameDir game_dir{QDir::toNativeSeparators(dir_path).toStdString(), false,
                                     true};
        if (!UISettings::values.game_dirs.contains(game_dir)) {
            UISettings::values.game_dirs.append(game_dir);
            changed = true;
            fill();
        }
    });
    connect(remove, &QPushButton::clicked, &dialog, [&] {
        auto* item = list->currentItem();
        if (item == nullptr) {
            return;
        }
        UISettings::GameDir game_dir{item->text().toStdString(), false, false};
        UISettings::values.game_dirs.removeOne(game_dir);
        changed = true;
        fill();
    });
    connect(open, &QPushButton::clicked, &dialog, [&] {
        if (auto* item = list->currentItem()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(item->text()));
        }
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addWidget(list, 1);
    layout->addLayout(buttons);
    dialog.exec();
    if (changed) {
        emit SaveConfigRequested();
        emit RefreshRequested();
    }
}

void Home::OpenQuickSettings() {
    QuickSettings dialog(this);
    connect(&dialog, &QuickSettings::SettingsChanged, this, &Home::SaveConfigRequested);
    connect(&dialog, &QuickSettings::OpenAllSettings, this, &Home::OpenSettingsRequested);
    dialog.exec();
}

void Home::AddGameActions(QMenu& menu, const QString& path, u64 program_id) {
    menu.addSeparator();
    QMenu* fenix = menu.addMenu(QIcon(QStringLiteral(":/img/fenix.png")), tr("Fenix"));
    QAction* play = fenix->addAction(tr("Play"));
    connect(play, &QAction::triggered, this,
            [this, path, program_id] { emit LaunchGame(path, program_id); });
    if (program_id != 0) {
        fenix->addSeparator();
        QAction* backup = fenix->addAction(tr("Back Up Saves Now"));
        connect(backup, &QAction::triggered, this, [this, program_id] {
            std::string error;
            const auto result = Saves::BackupNow(SaveRoot(), BackupRoot(), program_id,
                                                 Library::Get().BackupsToKeep(), &error);
            if (result) {
                QMessageBox::information(this, tr("Saves backed up"),
                                         tr("Backup saved to:\n%1")
                                             .arg(QString::fromStdString(
                                                 Common::FS::PathToUTF8String(*result))));
            } else {
                QMessageBox::information(
                    this, tr("No backup made"),
                    error.empty() ? tr("This game has no save data yet.")
                                  : QString::fromStdString(error));
            }
        });
        const Entry* entry = FindEntry(Library::KeyOf(path, program_id));
        const QString title = entry != nullptr ? entry->title : QFileInfo(path).completeBaseName();
        QAction* restore = fenix->addAction(tr("Restore a Save Backup..."));
        connect(restore, &QAction::triggered, this,
                [this, program_id, title] { ShowRestoreDialog(program_id, title); });
        QAction* open_backups = fenix->addAction(tr("Open Save Backups Folder"));
        connect(open_backups, &QAction::triggered, this, [program_id] {
            const auto folder = BackupRoot() / fmt::format("{:016X}", program_id);
            std::error_code ec;
            std::filesystem::create_directories(folder, ec);
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                QString::fromStdString(Common::FS::PathToUTF8String(folder))));
        });
        QAction* auto_backup = fenix->addAction(tr("Back Up Saves Before Every Launch"));
        auto_backup->setCheckable(true);
        auto_backup->setChecked(Library::Get().AutoBackup());
        connect(auto_backup, &QAction::toggled, this,
                [](bool on) { Library::Get().SetAutoBackup(on); });
    }
    fenix->addSeparator();
    QAction* front = fenix->addAction(tr("Move to Front (My order)"));
    connect(front, &QAction::triggered, this, [this, path, program_id] {
        const QString key = Library::KeyOf(path, program_id);
        QStringList order = Library::Get().CustomOrder();
        order.removeAll(key);
        order.prepend(key);
        Library::Get().SetCustomOrder(order);
        Library::Get().SetSort(SortMode::Custom);
        ApplyView();
    });
}

void Home::ShowRestoreDialog(u64 program_id, const QString& title) {
    const auto backups = Saves::List(BackupRoot(), program_id);
    if (backups.empty()) {
        QMessageBox::information(this, tr("No backups"),
                                 tr("%1 has no save backups yet. Fenix makes one every time the "
                                    "game starts (when it has save data).")
                                     .arg(title));
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("FenixRoot"));
    dialog.setStyleSheet(Theme::StyleSheet() +
                         QStringLiteral("QDialog#FenixRoot { background: #15151b; }"));
    dialog.setWindowTitle(tr("Restore a save backup"));
    dialog.resize(480, 380);
    auto* layout = new QVBoxLayout(&dialog);
    auto* label = new QLabel(tr("Pick a backup of %1 to restore. Your current saves are backed up "
                                "first, so this can be undone.")
                                 .arg(title),
                             &dialog);
    label->setWordWrap(true);
    auto* list = new QListWidget(&dialog);
    for (const auto& backup : backups) {
        list->addItem(QStringLiteral("%1   (%2 KB)")
                          .arg(QString::fromStdString(backup.label))
                          .arg(backup.bytes / 1024));
    }
    list->setCurrentRow(0);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto* restore = box->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
    restore->setObjectName(QStringLiteral("FenixPrimary"));
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(label);
    layout->addWidget(list, 1);
    layout->addWidget(box);
    if (dialog.exec() != QDialog::Accepted || list->currentRow() < 0) {
        return;
    }
    const auto& chosen = backups[static_cast<size_t>(list->currentRow())];
    std::string error;
    // Safety copy of what is there now (kept with the others).
    Saves::BackupNow(SaveRoot(), BackupRoot(), program_id, Library::Get().BackupsToKeep() + 1,
                     &error);
    error.clear();
    if (Saves::Restore(SaveRoot(), chosen, &error)) {
        QMessageBox::information(this, tr("Restored"),
                                 tr("%1's saves are back to %2.")
                                     .arg(title, QString::fromStdString(chosen.label)));
    } else {
        QMessageBox::warning(this, tr("Restore failed"), QString::fromStdString(error));
    }
}

void Home::BackupSavesBeforeBoot(u64 program_id) {
    if (program_id == 0 || !Library::Get().AutoBackup()) {
        return;
    }
    std::string error;
    const auto result = Saves::BackupNow(SaveRoot(), BackupRoot(), program_id,
                                         Library::Get().BackupsToKeep(), &error);
    if (result) {
        LOG_INFO(Frontend, "Fenix: saves of {:016X} backed up to {}", program_id,
                 Common::FS::PathToUTF8String(*result));
    } else if (!error.empty()) {
        LOG_WARNING(Frontend, "Fenix: save backup of {:016X} failed: {}", program_id, error);
    }
}

// --- input -----------------------------------------------------------------------------------

bool Home::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool ctrl = key->modifiers() & Qt::ControlModifier;
        if (object == search) {
            if (key->key() == Qt::Key_Escape || key->key() == Qt::Key_Down) {
                if (key->key() == Qt::Key_Escape) {
                    search->clear();
                }
                grid->setFocus();
                return true;
            }
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                grid->setFocus();
                LaunchCurrent();
                return true;
            }
            return false;
        }
        if (object == grid) {
            switch (key->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                LaunchCurrent();
                return true;
            case Qt::Key_F:
                if (ctrl) {
                    search->setFocus();
                    search->selectAll();
                } else {
                    ToggleFavoriteCurrent();
                }
                return true;
            case Qt::Key_S:
                CycleSort(1);
                return true;
            case Qt::Key_PageUp:
                CycleFilter(-1);
                return true;
            case Qt::Key_PageDown:
                CycleFilter(1);
                return true;
            case Qt::Key_Menu:
                OpenQuickSettings();
                return true;
            case Qt::Key_F5:
                emit RefreshRequested();
                return true;
            case Qt::Key_Slash:
                search->setFocus();
                return true;
            case Qt::Key_Escape:
                if (!search->text().isEmpty()) {
                    search->clear();
                    return true;
                }
                return false;
            case Qt::Key_Left:
            case Qt::Key_Right:
                if (ctrl) {
                    MoveCurrent(key->key() == Qt::Key_Left ? -1 : 1);
                    return true;
                }
                return false;
            case Qt::Key_Plus:
            case Qt::Key_Equal:
                if (ctrl) {
                    size_slider->setValue(size_slider->value() + 20);
                    return true;
                }
                return false;
            case Qt::Key_Minus:
                if (ctrl) {
                    size_slider->setValue(size_slider->value() - 20);
                    return true;
                }
                return false;
            default:
                break;
            }
        }
    }
    if (object == grid->viewport()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            drag_row = mouse->button() == Qt::LeftButton
                           ? grid->indexAt(mouse->position().toPoint()).row()
                           : -1;
            drag_start = mouse->position().toPoint();
            dragging = false;
            break;
        }
        case QEvent::MouseMove: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (drag_row >= 0 && (mouse->buttons() & Qt::LeftButton) && !dragging &&
                (mouse->position().toPoint() - drag_start).manhattanLength() >
                    QApplication::startDragDistance() * 2) {
                dragging = true;
                grid->viewport()->setCursor(Qt::ClosedHandCursor);
            }
            if (dragging) {
                const QModelIndex over = grid->indexAt(mouse->position().toPoint());
                if (over.isValid()) {
                    grid->setCurrentIndex(over);
                }
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            if (dragging) {
                auto* mouse = static_cast<QMouseEvent*>(event);
                const QModelIndex over = grid->indexAt(mouse->position().toPoint());
                grid->viewport()->unsetCursor();
                dragging = false;
                if (over.isValid()) {
                    MoveRow(drag_row, over.row());
                }
                drag_row = -1;
                return true;
            }
            drag_row = -1;
            break;
        }
        default:
            break;
        }
    }
    if (object == grid->viewport() && event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            size_slider->setValue(size_slider->value() + (wheel->angleDelta().y() > 0 ? 16 : -16));
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

} // namespace Fenix
