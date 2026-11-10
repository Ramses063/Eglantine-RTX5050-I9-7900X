// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cmath>
#include <initializer_list>

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

// Fenix's own line icons, drawn at any size and colour so they stay sharp on every display and
// need no image files. Each is designed on a 24 x 24 grid with a 2 unit round stroke.
namespace Fenix::Icons {

inline constexpr qreal Pi = 3.14159265358979323846;

enum class Glyph {
    Plus,
    Folder,
    Refresh,
    Sliders,
    Gear,
    Report,
    Search,
    Play,
    Star,
    StarOutline,
    Image,
    More,
    Grid,
    Home,
};

inline QPainterPath StarPath(qreal cx, qreal cy, qreal outer, qreal inner) {
    QPainterPath path;
    for (int i = 0; i < 10; ++i) {
        const qreal radius = i % 2 == 0 ? outer : inner;
        const qreal angle = -Pi / 2 + i * Pi / 5;
        const QPointF point(cx + radius * std::cos(angle), cy + radius * std::sin(angle));
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}

/// Paints a glyph into `rect` (a square) with the painter's current transform.
inline void Paint(QPainter& painter, Glyph glyph, const QRectF& rect, const QColor& color) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    painter.scale(rect.width() / 24.0, rect.height() / 24.0);
    QPen pen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    switch (glyph) {
    case Glyph::Plus:
        painter.drawLine(QPointF(12, 5), QPointF(12, 19));
        painter.drawLine(QPointF(5, 12), QPointF(19, 12));
        break;
    case Glyph::Folder: {
        QPainterPath path;
        path.moveTo(3, 8);
        path.quadTo(3, 5.5, 5.5, 5.5);
        path.lineTo(9, 5.5);
        path.lineTo(11.5, 8);
        path.lineTo(18.5, 8);
        path.quadTo(21, 8, 21, 10.5);
        path.lineTo(21, 16.5);
        path.quadTo(21, 19, 18.5, 19);
        path.lineTo(5.5, 19);
        path.quadTo(3, 19, 3, 16.5);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
    case Glyph::Refresh: {
        // Most of a circle, with an arrow head where it ends.
        const QRectF circle(5, 5, 14, 14);
        const qreal start = 50.0;
        const qreal sweep = 280.0;
        QPainterPath path;
        path.arcMoveTo(circle, start);
        path.arcTo(circle, start, sweep);
        painter.drawPath(path);
        const qreal end = (start + sweep) * Pi / 180.0;
        const QPointF tip(12 + 7 * std::cos(end), 12 - 7 * std::sin(end));
        // The head trails back along the arc, 40 degrees either side of it.
        painter.drawLine(tip, tip + QPointF(-3.95, 1.44));
        painter.drawLine(tip, tip + QPointF(0.73, 4.14));
        break;
    }
    case Glyph::Sliders: {
        const qreal rows[3] = {7, 12, 17};
        const qreal knobs[3] = {9, 15.5, 8};
        for (int i = 0; i < 3; ++i) {
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(QPointF(4, rows[i]), QPointF(knobs[i] - 3.2, rows[i]));
            painter.drawLine(QPointF(knobs[i] + 3.2, rows[i]), QPointF(20, rows[i]));
            painter.drawEllipse(QPointF(knobs[i], rows[i]), 2.1, 2.1);
        }
        break;
    }
    case Glyph::Gear: {
        painter.drawEllipse(QPointF(12, 12), 3.0, 3.0);
        painter.drawEllipse(QPointF(12, 12), 6.6, 6.6);
        QPen teeth(color, 2.6, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(teeth);
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * Pi / 4;
            painter.drawLine(QPointF(12 + 7.6 * std::cos(angle), 12 + 7.6 * std::sin(angle)),
                             QPointF(12 + 9.4 * std::cos(angle), 12 + 9.4 * std::sin(angle)));
        }
        break;
    }
    case Glyph::Report: {
        painter.drawRoundedRect(QRectF(5.5, 5, 13, 16), 2.4, 2.4);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(9, 3, 6, 3.6), 1.2, 1.2);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(9, 11.5), QPointF(15, 11.5));
        painter.drawLine(QPointF(9, 15.5), QPointF(13, 15.5));
        break;
    }
    case Glyph::Search:
        painter.drawEllipse(QPointF(10.5, 10.5), 5.6, 5.6);
        painter.drawLine(QPointF(15, 15), QPointF(20, 20));
        break;
    case Glyph::Play: {
        QPainterPath path;
        path.moveTo(8.5, 5.5);
        path.lineTo(18.5, 12);
        path.lineTo(8.5, 18.5);
        path.closeSubpath();
        painter.setBrush(color);
        painter.drawPath(path);
        break;
    }
    case Glyph::Star:
        painter.setBrush(color);
        painter.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(StarPath(12, 12.6, 8.6, 3.9));
        break;
    case Glyph::StarOutline:
        painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(StarPath(12, 12.6, 8.4, 3.9));
        break;
    case Glyph::Image: {
        painter.drawRoundedRect(QRectF(3.5, 5, 17, 14), 2.6, 2.6);
        painter.drawEllipse(QPointF(8.6, 9.8), 1.5, 1.5);
        QPainterPath path;
        path.moveTo(4.5, 17);
        path.lineTo(10, 12.2);
        path.lineTo(13.2, 15);
        path.lineTo(15.8, 12.8);
        path.lineTo(19.5, 16.4);
        painter.drawPath(path);
        break;
    }
    case Glyph::More:
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        for (const qreal x : {6.0, 12.0, 18.0}) {
            painter.drawEllipse(QPointF(x, 12), 1.7, 1.7);
        }
        break;
    case Glyph::Grid:
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        for (const qreal y : {4.0, 10.0, 16.0}) {
            for (const qreal x : {4.0, 10.0, 16.0}) {
                painter.drawRoundedRect(QRectF(x, y, 4.4, 4.4), 1.2, 1.2);
            }
        }
        break;
    case Glyph::Home: {
        QPainterPath path;
        path.moveTo(4, 11);
        path.lineTo(12, 4.5);
        path.lineTo(20, 11);
        painter.drawPath(path);
        QPainterPath body;
        body.moveTo(6.5, 9.5);
        body.lineTo(6.5, 19.5);
        body.lineTo(17.5, 19.5);
        body.lineTo(17.5, 9.5);
        painter.drawPath(body);
        painter.drawLine(QPointF(10.5, 19.5), QPointF(10.5, 14.5));
        painter.drawLine(QPointF(10.5, 14.5), QPointF(13.5, 14.5));
        painter.drawLine(QPointF(13.5, 14.5), QPointF(13.5, 19.5));
        break;
    }
    }
    painter.restore();
}

inline QPixmap Pixmap(Glyph glyph, int size, const QColor& color) {
    const qreal dpr = qGuiApp != nullptr ? qGuiApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    Paint(painter, glyph, QRectF(0, 0, size, size), color);
    return pixmap;
}

inline QIcon Icon(Glyph glyph, const QColor& color = QColor(0xf4, 0xf4, 0xf6), int size = 20) {
    QIcon icon;
    icon.addPixmap(Pixmap(glyph, size, color), QIcon::Normal);
    QColor disabled = color;
    disabled.setAlpha(90);
    icon.addPixmap(Pixmap(glyph, size, disabled), QIcon::Disabled);
    return icon;
}

} // namespace Fenix::Icons
