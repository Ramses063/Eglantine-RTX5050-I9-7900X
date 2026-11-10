// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QEasingCurve>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QLineF>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "yuzu/fenix/fenix_console.h"
#include "yuzu/fenix/fenix_icons.h"
#include "yuzu/fenix/fenix_theme.h"

namespace Fenix {

namespace {

constexpr int ButtonCount = static_cast<int>(ConsoleView::Action::Count);
constexpr qreal Lift = 0.07; ///< How much bigger the selected tile gets
const QColor Base{0x1d, 0x1d, 0x22};
const QColor Focus{0x2c, 0xc9, 0xff};
const QColor FocusLight{0xa8, 0xec, 0xff};

QString ActionLabel(ConsoleView::Action action) {
    switch (action) {
    case ConsoleView::Action::QuickSettings:
        return ConsoleView::tr("Quick Settings");
    case ConsoleView::Action::AddGames:
        return ConsoleView::tr("Add Games");
    case ConsoleView::Action::AllSoftware:
        return ConsoleView::tr("All Software");
    case ConsoleView::Action::Settings:
        return ConsoleView::tr("Settings");
    case ConsoleView::Action::DebugReport:
        return ConsoleView::tr("Debug Report");
    default:
        return {};
    }
}

Icons::Glyph ActionGlyph(ConsoleView::Action action) {
    switch (action) {
    case ConsoleView::Action::QuickSettings:
        return Icons::Glyph::Sliders;
    case ConsoleView::Action::AddGames:
        return Icons::Glyph::Plus;
    case ConsoleView::Action::AllSoftware:
        return Icons::Glyph::Grid;
    case ConsoleView::Action::Settings:
        return Icons::Glyph::Gear;
    case ConsoleView::Action::DebugReport:
    default:
        return Icons::Glyph::Report;
    }
}

QColor ActionColor(ConsoleView::Action action) {
    switch (action) {
    case ConsoleView::Action::QuickSettings:
        return QColor(0xff, 0x4a, 0x55);
    case ConsoleView::Action::AddGames:
        return QColor(0x4c, 0xd9, 0x7b);
    case ConsoleView::Action::AllSoftware:
        return QColor(0x5a, 0xa9, 0xff);
    case ConsoleView::Action::Settings:
        return QColor(0xd6, 0xd6, 0xde);
    case ConsoleView::Action::DebugReport:
    default:
        return QColor(0xff, 0xc8, 0x3d);
    }
}

QColor Mix(const QColor& a, const QColor& b, qreal t) {
    return QColor::fromRgbF(static_cast<float>(a.redF() + (b.redF() - a.redF()) * t),
                            static_cast<float>(a.greenF() + (b.greenF() - a.greenF()) * t),
                            static_cast<float>(a.blueF() + (b.blueF() - a.blueF()) * t),
                            static_cast<float>(a.alphaF() + (b.alphaF() - a.alphaF()) * t));
}

/// A calm version of an image's average color, for the background glow.
QColor TintOf(const QPixmap& icon) {
    if (icon.isNull()) {
        return QColor(40, 40, 48);
    }
    const QImage one = icon.toImage().scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QColor color = one.pixelColor(0, 0);
    float h, s, v, a;
    color.getHsvF(&h, &s, &v, &a);
    color.setHsvF(h < 0 ? 0 : h, std::min(s, 0.65f), std::clamp(v, 0.30f, 0.55f));
    return color;
}

/// Art for a game that has none: a gradient from its name, with the name on it.
QPixmap Placeholder(const QString& title, int size) {
    QPixmap pixmap(size, size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const int hue = static_cast<int>(qHash(title) % 360);
    QLinearGradient gradient(0, 0, size, size);
    gradient.setColorAt(0, QColor::fromHsv(hue, 150, 150));
    gradient.setColorAt(1, QColor::fromHsv((hue + 40) % 360, 170, 60));
    painter.fillRect(pixmap.rect(), gradient);
    QFont font;
    font.setPixelSize(std::max(12, size / 9));
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255, 235));
    const int pad = size / 10;
    painter.drawText(QRect(pad, pad, size - 2 * pad, size - 2 * pad),
                     Qt::AlignCenter | Qt::TextWordWrap, title);
    return pixmap;
}

} // namespace

ConsoleView::ConsoleView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("FenixConsole"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    logo = QPixmap(QStringLiteral(":/img/fenix.png"));

    scroll_anim = new QVariantAnimation(this);
    scroll_anim->setDuration(260);
    scroll_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(scroll_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        scroll = value.toReal();
        update();
    });

    grow_anim = new QVariantAnimation(this);
    grow_anim->setDuration(170);
    grow_anim->setEasingCurve(QEasingCurve::OutCubic);
    grow_anim->setStartValue(0.0);
    grow_anim->setEndValue(1.0);
    connect(grow_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        grow = value.toReal();
        update();
    });

    tint_anim = new QVariantAnimation(this);
    tint_anim->setDuration(450);
    tint_anim->setEasingCurve(QEasingCurve::InOutQuad);
    tint_anim->setStartValue(0.0);
    tint_anim->setEndValue(1.0);
    connect(tint_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        tint = Mix(tint_from, tint_to, value.toReal());
        update();
    });

    // The focus frame breathes; only its own area is repainted.
    pulse_timer = new QTimer(this);
    pulse_timer->setInterval(33);
    connect(pulse_timer, &QTimer::timeout, this, [this] {
        pulse = std::fmod(pulse + 0.033 / 1.4, 1.0);
        const Metrics m = Measure();
        const QRectF frame = row == 0 ? TileRect(m, selected) : ButtonRect(m, button);
        const qreal pad = 12 * m.unit;
        update(frame.adjusted(-pad, -pad, pad, pad).toAlignedRect());
    });

    clock_timer = new QTimer(this);
    clock_timer->setInterval(10'000);
    connect(clock_timer, &QTimer::timeout, this, [this] { update(0, 0, width(), height() / 6); });
}

ConsoleView::~ConsoleView() = default;

void ConsoleView::SetTiles(std::vector<Tile> new_tiles) {
    const QString key = SelectedKey();
    const bool on_special = !tiles.empty() && selected >= static_cast<int>(tiles.size());
    tiles = std::move(new_tiles);
    scaled_cache.clear();
    int index = 0;
    if (on_special) {
        index = static_cast<int>(tiles.size());
    } else if (!key.isEmpty()) {
        for (size_t i = 0; i < tiles.size(); ++i) {
            if (tiles[i].key == key) {
                index = static_cast<int>(i);
                break;
            }
        }
    }
    selected = std::clamp(index, 0, TileCount() - 1);
    previous = -1;
    grow = 1;
    ScrollToSelected(false);
    UpdateTint();
    update();
}

QString ConsoleView::SelectedKey() const {
    if (selected >= 0 && selected < static_cast<int>(tiles.size())) {
        return tiles[selected].key;
    }
    return {};
}

int ConsoleView::TileCount() const {
    return static_cast<int>(tiles.size()) + 1;
}

// --- geometry --------------------------------------------------------------------------------

ConsoleView::Metrics ConsoleView::Measure() const {
    Metrics m;
    const qreal w = width();
    const qreal h = height();
    m.unit = std::clamp(std::min(h / 900.0, w / 1440.0), 0.6, 1.8);
    m.margin = 64 * m.unit;
    m.tile = std::clamp(std::min(h * 0.32, w * 0.205), 140.0, 420.0);
    m.gap = std::round(m.tile * 0.075);
    m.button_size = std::round(70 * m.unit);
    m.button_gap = std::round(30 * m.unit);
    m.bottom_y = h - 72 * m.unit;

    const qreal top = 112 * m.unit;
    const qreal title_h = 52 * m.unit;
    const qreal block = title_h + m.tile * (1 + Lift) + 64 * m.unit + m.button_size + 36 * m.unit;
    const qreal start = top + std::max(0.0, (m.bottom_y - top - block) * 0.4);
    m.title_y = start + title_h - 16 * m.unit;
    m.row_top = start + title_h + m.tile * Lift / 2;
    m.button_y = m.row_top + m.tile * (1 + Lift / 2) + 64 * m.unit;
    return m;
}

QRectF ConsoleView::TileRect(const Metrics& m, int index) const {
    const qreal x = m.margin + index * (m.tile + m.gap) - scroll;
    QRectF rect(x, m.row_top, m.tile, m.tile);
    qreal lift = 0;
    const int lifted = row == 0 ? selected : -1;
    if (index == lifted) {
        lift = Lift * grow;
    } else if (index == previous) {
        lift = Lift * (1 - grow);
    }
    if (lift > 0) {
        const qreal d = m.tile * lift / 2;
        rect.adjust(-d, -d, d, d);
    }
    return rect;
}

QRectF ConsoleView::ButtonRect(const Metrics& m, int index) const {
    const qreal total = ButtonCount * m.button_size + (ButtonCount - 1) * m.button_gap;
    const qreal x = (width() - total) / 2 + index * (m.button_size + m.button_gap);
    return QRectF(x, m.button_y, m.button_size, m.button_size);
}

int ConsoleView::TileAt(const QPointF& pos) const {
    const Metrics m = Measure();
    for (int i = 0; i < TileCount(); ++i) {
        if (TileRect(m, i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

int ConsoleView::ButtonAt(const QPointF& pos) const {
    const Metrics m = Measure();
    for (int i = 0; i < ButtonCount; ++i) {
        const QRectF rect = ButtonRect(m, i);
        if (QLineF(rect.center(), pos).length() <= rect.width() / 2 + 2) {
            return i;
        }
    }
    return -1;
}

// --- state -----------------------------------------------------------------------------------

void ConsoleView::Select(int tile) {
    tile = std::clamp(tile, 0, TileCount() - 1);
    if (tile == selected && row == 0) {
        return;
    }
    previous = row == 0 ? selected : -1;
    selected = tile;
    row = 0;
    grow = 0;
    grow_anim->stop();
    grow_anim->start();
    ScrollToSelected(true);
    UpdateTint();
    update();
}

void ConsoleView::SelectButton(int index) {
    index = std::clamp(index, 0, ButtonCount - 1);
    if (row == 1 && index == button) {
        return;
    }
    button = index;
    SetRow(1);
    update();
}

void ConsoleView::SetRow(int new_row) {
    if (row == new_row) {
        return;
    }
    // The tile shrinks back when the focus moves down, and grows again when it comes back.
    previous = row == 0 ? selected : -1;
    row = new_row;
    grow = 0;
    grow_anim->stop();
    grow_anim->start();
    update();
}

void ConsoleView::Activate() {
    if (row == 1) {
        ActivateButton(button);
        return;
    }
    if (selected < static_cast<int>(tiles.size())) {
        if (on_launch) {
            on_launch(tiles[selected].path, tiles[selected].program_id);
        }
        return;
    }
    if (on_action) {
        on_action(tiles.empty() ? Action::AddGames : Action::AllSoftware);
    }
}

void ConsoleView::ActivateButton(int index) {
    if (on_action && index >= 0 && index < ButtonCount) {
        on_action(static_cast<Action>(index));
    }
}

void ConsoleView::OpenMenu(const QPoint& global_pos) {
    if (row != 0 || selected >= static_cast<int>(tiles.size()) || !on_menu) {
        return;
    }
    on_menu(tiles[selected].path, tiles[selected].program_id, global_pos);
}

void ConsoleView::ScrollToSelected(bool animate) {
    const Metrics m = Measure();
    const qreal step = m.tile + m.gap;
    const qreal visible = std::max(step, width() - 2 * m.margin);
    const qreal content = TileCount() * step - m.gap;
    const qreal max_scroll = std::max(0.0, content - visible);
    const qreal left = selected * step;
    qreal target = scroll_target;
    if (left - target < 0) {
        target = left;
    } else if (left + m.tile - target > visible) {
        target = left + m.tile - visible;
    }
    target = std::clamp(target, 0.0, max_scroll);
    scroll_target = target;
    scroll_anim->stop();
    if (!animate || !isVisible()) {
        scroll = target;
        return;
    }
    scroll_anim->setStartValue(scroll);
    scroll_anim->setEndValue(target);
    scroll_anim->start();
}

void ConsoleView::UpdateTint() {
    const QColor target = selected < static_cast<int>(tiles.size())
                              ? TintOf(tiles[selected].icon)
                              : QColor(40, 40, 48);
    if (target == tint_to) {
        return;
    }
    tint_from = tint;
    tint_to = target;
    tint_anim->stop();
    if (!isVisible()) {
        tint = target;
        return;
    }
    tint_anim->start();
}

const QPixmap& ConsoleView::Scaled(const Tile& tile, int size) {
    const qreal dpr = devicePixelRatioF();
    const int pixels = static_cast<int>(std::round(size * dpr));
    const QString cache_key = tile.key + QLatin1Char('@') + QString::number(pixels);
    auto it = scaled_cache.find(cache_key);
    if (it == scaled_cache.end()) {
        QPixmap pixmap = tile.icon.isNull()
                             ? Placeholder(tile.title, pixels)
                             : tile.icon.scaled(pixels, pixels, Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(dpr);
        it = scaled_cache.insert(cache_key, pixmap);
    }
    return *it;
}

// --- painting --------------------------------------------------------------------------------

void ConsoleView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const Metrics m = Measure();

    // A dark base lit softly by the selected game's colors.
    QLinearGradient base(0, 0, 0, height());
    base.setColorAt(0, Mix(Base, tint, 0.22));
    base.setColorAt(0.6, Base);
    base.setColorAt(1, QColor(0x15, 0x15, 0x19));
    painter.fillRect(rect(), base);
    QRadialGradient glow(QPointF(width() * 0.5, m.row_top + m.tile * 0.5), width() * 0.6);
    QColor glow_color = tint;
    glow_color.setAlpha(70);
    glow.setColorAt(0, glow_color);
    glow_color.setAlpha(0);
    glow.setColorAt(1, glow_color);
    painter.fillRect(rect(), glow);

    PaintTopBar(painter, m);
    PaintTiles(painter, m);
    PaintButtons(painter, m);
    PaintBottomBar(painter, m);
}

void ConsoleView::PaintTopBar(QPainter& painter, const Metrics& m) {
    // Profile: a round badge with the Fenix logo.
    const qreal size = 54 * m.unit;
    const QRectF badge(m.margin - 8 * m.unit, 34 * m.unit, size, size);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 30));
    painter.drawEllipse(badge.adjusted(-3 * m.unit, -3 * m.unit, 3 * m.unit, 3 * m.unit));
    QLinearGradient fill(badge.topLeft(), badge.bottomRight());
    fill.setColorAt(0, QColor(0xff, 0x5a, 0x3c));
    fill.setColorAt(1, QColor(0xc4, 0x00, 0x2a));
    painter.setBrush(fill);
    painter.drawEllipse(badge);
    if (!logo.isNull()) {
        const QRectF inner = badge.adjusted(size * 0.16, size * 0.16, -size * 0.16, -size * 0.16);
        painter.drawPixmap(inner, logo, logo.rect());
    }

    // Clock on the right.
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale locale;
    QFont clock_font = font();
    clock_font.setPixelSize(static_cast<int>(26 * m.unit));
    clock_font.setWeight(QFont::DemiBold);
    painter.setFont(clock_font);
    painter.setPen(Theme::Text);
    QString time_format = locale.timeFormat(QLocale::ShortFormat);
    time_format.remove(QStringLiteral(":ss"));
    const QString time = locale.toString(now.time(), time_format);
    const QRectF clock_rect(0, badge.top(), width() - m.margin, badge.height() * 0.62);
    painter.drawText(clock_rect, Qt::AlignRight | Qt::AlignVCenter, time);
    QFont date_font = font();
    date_font.setPixelSize(static_cast<int>(14 * m.unit));
    painter.setFont(date_font);
    painter.setPen(Theme::TextMuted);
    painter.drawText(QRectF(0, clock_rect.bottom(), width() - m.margin, badge.height() * 0.4),
                     Qt::AlignRight | Qt::AlignVCenter,
                     locale.toString(now.date(), QStringLiteral("ddd d MMM")));
}

void ConsoleView::PaintFocusFrame(QPainter& painter, const QRectF& rect, qreal radius,
                                  qreal unit) {
    const qreal wave = 0.5 - 0.5 * std::cos(pulse * 2 * Icons::Pi);
    const QColor color = Mix(Focus, FocusLight, wave);
    const qreal width = 4.5 * unit;
    const qreal gap = 4 * unit;
    const QRectF frame = rect.adjusted(-gap - width / 2, -gap - width / 2, gap + width / 2,
                                       gap + width / 2);
    QColor halo = color;
    halo.setAlpha(60);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(halo, width * 3));
    painter.drawRoundedRect(frame, radius + gap, radius + gap);
    painter.setPen(QPen(color, width));
    painter.drawRoundedRect(frame, radius + gap, radius + gap);
}

void ConsoleView::PaintTiles(QPainter& painter, const Metrics& m) {
    const int count = TileCount();
    const int games = static_cast<int>(tiles.size());
    const qreal radius = m.tile * 0.055;
    // Paint the lifted tile last so it sits on top of its neighbours.
    std::vector<int> order;
    order.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (i != selected) {
            order.push_back(i);
        }
    }
    order.push_back(selected);

    for (const int i : order) {
        const QRectF rect = TileRect(m, i);
        if (rect.right() < -40 || rect.left() > width() + 40) {
            continue;
        }
        // Soft shadow under the tile.
        painter.setPen(Qt::NoPen);
        for (int s = 3; s >= 1; --s) {
            painter.setBrush(QColor(0, 0, 0, 22));
            const qreal d = s * 3 * m.unit;
            painter.drawRoundedRect(rect.adjusted(-d / 2, d / 2, d / 2, d * 1.4), radius + d,
                                    radius + d);
        }
        QPainterPath clip;
        clip.addRoundedRect(rect, radius, radius);
        if (i < games) {
            const QPixmap& art = Scaled(tiles[i], static_cast<int>(std::ceil(rect.width())));
            painter.save();
            painter.setClipPath(clip);
            painter.drawPixmap(rect, art, QRectF(QPointF(0, 0), art.size()));
            painter.restore();
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255, 26), 1));
            painter.drawPath(clip);
            if (tiles[i].favorite) {
                const qreal star = rect.width() * 0.13;
                const QRectF star_rect(rect.right() - star - rect.width() * 0.05,
                                       rect.top() + rect.width() * 0.05, star, star);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 120));
                painter.drawEllipse(star_rect.adjusted(-star * 0.18, -star * 0.18, star * 0.18,
                                                       star * 0.18));
                Icons::Paint(painter, Icons::Glyph::Star, star_rect, Theme::Gold);
            }
        } else {
            // All Software (or Add Games when the library is empty).
            const bool add = games == 0;
            painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
            painter.setBrush(QColor(0x34, 0x34, 0x3c));
            painter.drawPath(clip);
            const qreal glyph = rect.width() * 0.30;
            Icons::Paint(painter, add ? Icons::Glyph::Plus : Icons::Glyph::Grid,
                         QRectF(rect.center().x() - glyph / 2, rect.top() + rect.height() * 0.24,
                                glyph, glyph),
                         add ? QColor(0x4c, 0xd9, 0x7b) : QColor(0xe8, 0xe8, 0xee));
            QFont label_font = font();
            label_font.setPixelSize(std::max(11, static_cast<int>(rect.width() * 0.075)));
            label_font.setWeight(QFont::DemiBold);
            painter.setFont(label_font);
            painter.setPen(Theme::Text);
            painter.drawText(QRectF(rect.left(), rect.top() + rect.height() * 0.62, rect.width(),
                                    rect.height() * 0.2),
                             Qt::AlignCenter, add ? tr("Add Games") : tr("All Software"));
        }
        if (i == selected && row == 0) {
            PaintFocusFrame(painter, rect, radius, m.unit);
        }
    }

    // The selected game's name above it.
    if (row == 0) {
        const QRectF rect = TileRect(m, selected);
        QString title;
        if (selected < games) {
            title = tiles[selected].title;
        } else {
            title = games == 0 ? tr("Add your games to get started") : tr("All Software");
        }
        QFont title_font = font();
        title_font.setPixelSize(static_cast<int>(25 * m.unit));
        title_font.setWeight(QFont::DemiBold);
        painter.setFont(title_font);
        const QFontMetricsF metrics(title_font);
        const qreal available = width() - 2 * m.margin;
        const QString shown = metrics.elidedText(title, Qt::ElideRight, available);
        const qreal text_width = metrics.horizontalAdvance(shown);
        const qreal x = std::clamp(rect.left(), m.margin, width() - m.margin - text_width);
        painter.setPen(Mix(Theme::Text, FocusLight, 0.25));
        painter.drawText(QPointF(x, m.title_y), shown);
    }
}

void ConsoleView::PaintButtons(QPainter& painter, const Metrics& m) {
    for (int i = 0; i < ButtonCount; ++i) {
        const QRectF rect = ButtonRect(m, i);
        const auto action = static_cast<Action>(i);
        const bool focused = row == 1 && i == button;
        const bool hovered = i == hover_button;
        painter.setPen(QPen(QColor(255, 255, 255, hovered ? 60 : 28), 1));
        painter.setBrush(hovered || focused ? QColor(0x45, 0x45, 0x50) : QColor(0x36, 0x36, 0x3e));
        painter.drawEllipse(rect);
        const qreal glyph = rect.width() * 0.46;
        Icons::Paint(painter, ActionGlyph(action),
                     QRectF(rect.center().x() - glyph / 2, rect.center().y() - glyph / 2, glyph,
                            glyph),
                     ActionColor(action));
        if (focused) {
            const qreal wave = 0.5 - 0.5 * std::cos(pulse * 2 * Icons::Pi);
            const QColor color = Mix(Focus, FocusLight, wave);
            const qreal pen = 4 * m.unit;
            const qreal gap = 4 * m.unit + pen / 2;
            QColor halo = color;
            halo.setAlpha(60);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(halo, pen * 3));
            painter.drawEllipse(rect.adjusted(-gap, -gap, gap, gap));
            painter.setPen(QPen(color, pen));
            painter.drawEllipse(rect.adjusted(-gap, -gap, gap, gap));
        }
        if (focused || hovered) {
            QFont label_font = font();
            label_font.setPixelSize(static_cast<int>(16 * m.unit));
            label_font.setWeight(QFont::DemiBold);
            painter.setFont(label_font);
            painter.setPen(focused ? Mix(Theme::Text, FocusLight, 0.35) : Theme::Text);
            painter.drawText(QRectF(rect.center().x() - 120 * m.unit, rect.bottom() + 12 * m.unit,
                                    240 * m.unit, 24 * m.unit),
                             Qt::AlignCenter, ActionLabel(action));
        }
    }
}

void ConsoleView::PaintBottomBar(QPainter& painter, const Metrics& m) {
    painter.setPen(QPen(QColor(255, 255, 255, 38), 1));
    painter.drawLine(QPointF(m.margin * 0.6, m.bottom_y), QPointF(width() - m.margin * 0.6, m.bottom_y));

    QFont text_font = font();
    text_font.setPixelSize(static_cast<int>(15 * m.unit));
    QFont key_font = text_font;
    key_font.setPixelSize(static_cast<int>(13 * m.unit));
    key_font.setWeight(QFont::Bold);
    const QFontMetricsF text_metrics(text_font);
    const qreal cy = m.bottom_y + (height() - m.bottom_y) / 2;
    const qreal cap = 26 * m.unit;

    // Left: what this is.
    painter.setFont(text_font);
    painter.setPen(Theme::TextMuted);
    painter.drawText(QRectF(m.margin, m.bottom_y, width() / 2.0, height() - m.bottom_y),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Fenix"));

    struct Hint {
        QString key;
        QString text;
    };
    std::vector<Hint> hints;
    const bool game = row == 0 && selected < static_cast<int>(tiles.size());
    if (game) {
        hints.push_back({QStringLiteral("Y"), tr("Options")});
        hints.push_back({QStringLiteral("X"), tr("Favorite")});
    }
    hints.push_back({QStringLiteral("+"), tr("Quick Settings")});
    hints.push_back({QStringLiteral("A"), game ? tr("Start") : tr("OK")});

    // Right aligned, last hint rightmost.
    qreal x = width() - m.margin;
    for (auto it = hints.rbegin(); it != hints.rend(); ++it) {
        const qreal text_width = text_metrics.horizontalAdvance(it->text);
        x -= text_width;
        painter.setFont(text_font);
        painter.setPen(Theme::Text);
        painter.drawText(QRectF(x, m.bottom_y, text_width + 2, height() - m.bottom_y),
                         Qt::AlignLeft | Qt::AlignVCenter, it->text);
        x -= 8 * m.unit + cap;
        const QRectF circle(x, cy - cap / 2, cap, cap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xf4, 0xf4, 0xf6));
        painter.drawEllipse(circle);
        painter.setFont(key_font);
        painter.setPen(Base);
        painter.drawText(circle, Qt::AlignCenter, it->key);
        x -= 26 * m.unit;
    }
}

// --- input -----------------------------------------------------------------------------------

void ConsoleView::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left:
        if (row == 0) {
            Select(selected - 1);
        } else {
            SelectButton(button - 1);
        }
        return;
    case Qt::Key_Right:
        if (row == 0) {
            Select(selected + 1);
        } else {
            SelectButton(button + 1);
        }
        return;
    case Qt::Key_Down:
        if (row == 0) {
            SetRow(1);
        }
        return;
    case Qt::Key_Up:
    case Qt::Key_Escape:
        if (row == 1) {
            SetRow(0);
        }
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        Activate();
        return;
    case Qt::Key_Home:
    case Qt::Key_PageUp:
        Select(0);
        return;
    case Qt::Key_End:
    case Qt::Key_PageDown:
        Select(TileCount() - 1);
        return;
    case Qt::Key_Menu:
        ActivateButton(static_cast<int>(Action::QuickSettings));
        return;
    case Qt::Key_S: { // Y on a controller
        const Metrics m = Measure();
        const QRectF rect = TileRect(m, selected);
        OpenMenu(mapToGlobal(rect.center().toPoint()));
        return;
    }
    case Qt::Key_F: // X on a controller
        if (row == 0 && selected < static_cast<int>(tiles.size()) && on_favorite) {
            on_favorite(tiles[selected].program_id);
        }
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

void ConsoleView::mousePressEvent(QMouseEvent* event) {
    setFocus();
    const QPointF pos = event->position();
    if (const int tile = TileAt(pos); tile >= 0) {
        const bool again = row == 0 && tile == selected;
        Select(tile);
        if (event->button() == Qt::RightButton) {
            OpenMenu(event->globalPosition().toPoint());
        } else if (event->button() == Qt::LeftButton && again) {
            Activate();
        }
        return;
    }
    if (const int index = ButtonAt(pos); index >= 0 && event->button() == Qt::LeftButton) {
        SelectButton(index);
        ActivateButton(index);
        return;
    }
    QWidget::mousePressEvent(event);
}

void ConsoleView::mouseDoubleClickEvent(QMouseEvent* event) {
    // The first click already selected the tile and the second press launches it.
    mousePressEvent(event);
}

void ConsoleView::mouseMoveEvent(QMouseEvent* event) {
    const int index = ButtonAt(event->position());
    if (index != hover_button) {
        hover_button = index;
        setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    if (index < 0 && TileAt(event->position()) >= 0) {
        setCursor(Qt::PointingHandCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void ConsoleView::leaveEvent(QEvent* event) {
    hover_button = -1;
    update();
    QWidget::leaveEvent(event);
}

void ConsoleView::wheelEvent(QWheelEvent* event) {
    const QPoint delta = event->angleDelta();
    const int step = (delta.y() != 0 ? delta.y() : delta.x()) > 0 ? -1 : 1;
    Select(selected + step);
    event->accept();
}

void ConsoleView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    scaled_cache.clear();
    ScrollToSelected(false);
}

void ConsoleView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    pulse_timer->start();
    clock_timer->start();
    ScrollToSelected(false);
}

void ConsoleView::hideEvent(QHideEvent* event) {
    pulse_timer->stop();
    clock_timer->stop();
    QWidget::hideEvent(event);
}

} // namespace Fenix
