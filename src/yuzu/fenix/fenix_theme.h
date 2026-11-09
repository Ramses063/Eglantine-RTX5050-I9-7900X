// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QString>

// Fenix's look: a dark base with Nintendo-style red accents. Every Fenix widget takes its colours
// from here so the home screen, the dialogs and the cards match.
namespace Fenix::Theme {

inline const QColor Background{0x10, 0x10, 0x14};
inline const QColor Surface{0x1b, 0x1b, 0x22};
inline const QColor SurfaceHover{0x26, 0x26, 0x30};
inline const QColor SurfaceRaised{0x2c, 0x2c, 0x38};
inline const QColor Border{0x33, 0x33, 0x40};
inline const QColor Red{0xe6, 0x00, 0x12};
inline const QColor RedBright{0xff, 0x2d, 0x3d};
inline const QColor RedDark{0x9a, 0x00, 0x0c};
inline const QColor Text{0xf4, 0xf4, 0xf6};
inline const QColor TextMuted{0x9a, 0x9a, 0xa8};
inline const QColor Gold{0xff, 0xc8, 0x3d};
inline const QColor Switch1Badge{0x3a, 0x3a, 0x48};

/// The stylesheet for the Fenix home screen and dialogs (scoped by object name).
inline QString StyleSheet() {
    return QStringLiteral(R"(
#FenixRoot, #FenixRoot QWidget#FenixPage { background: #101014; }
#FenixRoot QLabel { color: #f4f4f6; }
#FenixRoot QLabel#FenixMuted { color: #9a9aa8; }
#FenixRoot QLabel#FenixBrand { color: #ffffff; font-size: 22px; font-weight: 800; letter-spacing: 3px; }
#FenixRoot QLabel#FenixSubtitle { color: #9a9aa8; font-size: 12px; }
#FenixRoot QLabel#FenixSection { color: #f4f4f6; font-size: 16px; font-weight: 700; }
#FenixRoot QLabel#FenixHeroTitle { color: #ffffff; font-size: 24px; font-weight: 800; }
#FenixRoot QLabel#FenixHeroKicker { color: #ff2d3d; font-size: 12px; font-weight: 700; letter-spacing: 2px; }
#FenixRoot QLabel#FenixEmptyTitle { color: #ffffff; font-size: 26px; font-weight: 800; }
#FenixRoot QFrame#FenixTopBar { background: #15151b; border-bottom: 1px solid #26262f; }
#FenixRoot QFrame#FenixHero {
    border-radius: 18px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3a0007, stop:0.55 #1b1b22, stop:1 #15151b);
    border: 1px solid #3a1a20;
}
#FenixRoot QLineEdit {
    background: #1b1b22; color: #f4f4f6; border: 1px solid #33333f; border-radius: 18px;
    padding: 8px 14px; font-size: 13px; selection-background-color: #e60012;
}
#FenixRoot QLineEdit:focus { border: 1px solid #e60012; }
#FenixRoot QPushButton, #FenixRoot QToolButton {
    background: #1f1f27; color: #f4f4f6; border: 1px solid #33333f; border-radius: 16px;
    padding: 7px 14px; font-size: 13px; font-weight: 600;
}
#FenixRoot QPushButton:hover, #FenixRoot QToolButton:hover { background: #2a2a35; border-color: #4a4a58; }
#FenixRoot QPushButton:focus, #FenixRoot QToolButton:focus { border: 2px solid #e60012; }
#FenixRoot QPushButton:pressed, #FenixRoot QToolButton:pressed { background: #9a000c; }
#FenixRoot QPushButton#FenixPrimary {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff2d3d, stop:1 #c4000f);
    border: none; color: white; font-weight: 800; padding: 9px 22px; border-radius: 16px;
    min-height: 18px;
}
#FenixRoot QPushButton#FenixPrimary:hover { background: #ff3b4a; }
#FenixRoot QPushButton#FenixChip { border-radius: 15px; padding: 6px 14px; background: #1b1b22; }
#FenixRoot QPushButton#FenixChip:checked { background: #e60012; border-color: #e60012; color: white; }
#FenixRoot QComboBox {
    background: #1f1f27; color: #f4f4f6; border: 1px solid #33333f; border-radius: 15px;
    padding: 6px 12px; font-size: 13px;
}
#FenixRoot QComboBox:focus { border: 2px solid #e60012; }
#FenixRoot QComboBox QAbstractItemView {
    background: #1f1f27; color: #f4f4f6; selection-background-color: #e60012; border: 1px solid #33333f;
}
#FenixRoot QListView { background: transparent; border: none; outline: none; }
#FenixRoot QScrollBar:vertical { background: transparent; width: 10px; margin: 4px; }
#FenixRoot QScrollBar::handle:vertical { background: #3a3a48; border-radius: 4px; min-height: 40px; }
#FenixRoot QScrollBar::handle:vertical:hover { background: #e60012; }
#FenixRoot QScrollBar::add-line, #FenixRoot QScrollBar::sub-line { height: 0; width: 0; }
#FenixRoot QScrollBar::add-page, #FenixRoot QScrollBar::sub-page { background: transparent; }
#FenixRoot QSlider::groove:horizontal { height: 4px; background: #33333f; border-radius: 2px; }
#FenixRoot QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: #e60012; }
#FenixRoot QCheckBox { color: #f4f4f6; }
#FenixRoot QListWidget { background: #1b1b22; color: #f4f4f6; border: 1px solid #33333f; border-radius: 10px; }
#FenixRoot QListWidget::item { padding: 8px; }
#FenixRoot QListWidget::item:selected { background: #e60012; color: white; }
)");
}

} // namespace Fenix::Theme
