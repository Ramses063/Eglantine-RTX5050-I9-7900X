// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPalette>
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

/// The palette of the app-wide Fenix theme (used with the Fusion style and :/fenix/style.qss).
inline QPalette AppPalette() {
    QPalette palette;
    const QColor window{0x14, 0x14, 0x18};
    const QColor base{0x1b, 0x1b, 0x22};
    const QColor button{0x25, 0x25, 0x2e};
    const QColor text{0xf1, 0xf1, 0xf4};
    const QColor disabled{0x6f, 0x6f, 0x7c};
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, QColor{0x20, 0x20, 0x28});
    palette.setColor(QPalette::ToolTipBase, QColor{0x26, 0x26, 0x2f});
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::PlaceholderText, QColor{0x7d, 0x7d, 0x8b});
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Light, QColor{0x3a, 0x3a, 0x47});
    palette.setColor(QPalette::Midlight, QColor{0x30, 0x30, 0x3b});
    palette.setColor(QPalette::Mid, QColor{0x2a, 0x2a, 0x34});
    palette.setColor(QPalette::Dark, QColor{0x0e, 0x0e, 0x12});
    palette.setColor(QPalette::Shadow, Qt::black);
    palette.setColor(QPalette::Highlight, Red);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor{0xff, 0x5a, 0x66});
    palette.setColor(QPalette::LinkVisited, QColor{0xd9, 0x6a, 0x8f});
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, disabled);
    }
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor{0x4a, 0x2a, 0x2e});
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor{0x17, 0x17, 0x1c});
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor{0x1c, 0x1c, 0x23});
    return palette;
}

/// The stylesheet for the Fenix home screen and dialogs (scoped by object name).
inline QString StyleSheet() {
    return QStringLiteral(R"(
#FenixRoot { background: #101014; }
#FenixRoot QWidget#FenixPage { background: transparent; }
#FenixRoot QLabel { color: #f4f4f6; background: transparent; }
#FenixRoot QLabel#FenixMuted { color: #9a9aa8; }
#FenixRoot QLabel#FenixBrand { color: #ffffff; font-size: 22px; font-weight: 800; letter-spacing: 4px; }
#FenixRoot QLabel#FenixSubtitle { color: #9a9aa8; font-size: 12px; }
#FenixRoot QLabel#FenixSection { color: #ffffff; font-size: 20px; font-weight: 800; }
#FenixRoot QLabel#FenixHeroTitle { color: #ffffff; font-size: 30px; font-weight: 800; }
#FenixRoot QLabel#FenixHeroKicker { color: #ff5a66; font-size: 12px; font-weight: 800; letter-spacing: 3px; }
#FenixRoot QLabel#FenixEmptyTitle { color: #ffffff; font-size: 28px; font-weight: 800; }
#FenixRoot QLabel#FenixPill, #FenixRoot QLabel#FenixPillRed {
    color: #f4f4f6; font-size: 12px; font-weight: 600; border-radius: 12px; padding: 0 12px;
    min-height: 24px; max-height: 24px; background: rgba(255, 255, 255, 34);
}
#FenixRoot QLabel#FenixPillRed {
    background: #e60012; color: white; font-size: 10px; font-weight: 800; letter-spacing: 1px;
}
#FenixRoot QFrame#FenixTopBar {
    background: rgba(12, 12, 16, 150); border: none; border-bottom: 1px solid rgba(255, 255, 255, 18);
}
#FenixRoot QLineEdit {
    background: #1b1b22; color: #f4f4f6; border: 1px solid #33333f; border-radius: 10px;
    padding: 8px 12px; font-size: 13px; selection-background-color: #e60012;
}
#FenixRoot QLineEdit:focus { border: 1px solid #e60012; }
#FenixRoot QLineEdit#FenixSearch {
    background: rgba(255, 255, 255, 16); border: 1px solid rgba(255, 255, 255, 26);
    border-radius: 20px; padding: 0 14px 0 8px; font-size: 14px;
}
#FenixRoot QLineEdit#FenixSearch:hover { background: rgba(255, 255, 255, 24); }
#FenixRoot QLineEdit#FenixSearch:focus { border: 1px solid #ff2d3d; background: rgba(0, 0, 0, 90); }
#FenixRoot QPushButton, #FenixRoot QToolButton {
    background: rgba(255, 255, 255, 16); color: #f4f4f6; border: 1px solid rgba(255, 255, 255, 26);
    border-radius: 19px; padding: 0 17px; font-size: 13px; font-weight: 600;
    min-height: 36px; max-height: 36px;
}
#FenixRoot QPushButton:hover, #FenixRoot QToolButton:hover {
    background: rgba(255, 255, 255, 32); border-color: rgba(255, 255, 255, 60);
}
#FenixRoot QPushButton:focus, #FenixRoot QToolButton:focus {
    border: 2px solid #ff2d3d; padding: 0 16px; min-height: 34px; max-height: 34px;
}
#FenixRoot QPushButton:pressed, #FenixRoot QToolButton:pressed { background: #9a000c; }
#FenixRoot QPushButton:checked { background: #e60012; border-color: #e60012; color: white; }
#FenixRoot QPushButton:disabled { color: #6f6f7c; background: rgba(255, 255, 255, 8); }
#FenixRoot QFrame#FenixTopBar QPushButton {
    border-radius: 20px; min-height: 38px; max-height: 38px;
}
#FenixRoot QFrame#FenixTopBar QPushButton:focus { min-height: 36px; max-height: 36px; }
#FenixRoot QPushButton#FenixIconButton {
    padding: 0; min-width: 38px; max-width: 38px;
}
#FenixRoot QPushButton#FenixIconButton:focus { padding: 0; min-width: 36px; max-width: 36px; }
#FenixRoot QPushButton#FenixPrimary {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff3545, stop:1 #d20011);
    border: 1px solid #ff5a66; color: white; font-size: 15px; font-weight: 800; padding: 0 24px;
    border-radius: 23px; min-height: 44px; max-height: 44px;
}
#FenixRoot QPushButton#FenixPrimary:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff5462, stop:1 #e60012);
}
#FenixRoot QPushButton#FenixPrimary:pressed { background: #b0000e; }
#FenixRoot QPushButton#FenixPrimary:focus {
    border: 2px solid #ffffff; padding: 0 23px; min-height: 42px; max-height: 42px;
}
#FenixRoot QPushButton#FenixGlass {
    background: rgba(255, 255, 255, 30); border: 1px solid rgba(255, 255, 255, 50);
    border-radius: 23px; font-size: 14px; padding: 0 18px; min-height: 44px; max-height: 44px;
}
#FenixRoot QPushButton#FenixGlass:hover { background: rgba(255, 255, 255, 56); }
#FenixRoot QPushButton#FenixGlass:focus {
    border: 2px solid #ffffff; padding: 0 17px; min-height: 42px; max-height: 42px;
}
#FenixRoot QPushButton#FenixGlassIcon {
    background: rgba(255, 255, 255, 30); border: 1px solid rgba(255, 255, 255, 50);
    border-radius: 23px; padding: 0; min-height: 44px; max-height: 44px; min-width: 44px;
    max-width: 44px;
}
#FenixRoot QPushButton#FenixGlassIcon:hover { background: rgba(255, 255, 255, 56); }
#FenixRoot QPushButton#FenixGlassIcon:focus {
    border: 2px solid #ffffff; padding: 0; min-height: 42px; max-height: 42px; min-width: 42px;
    max-width: 42px;
}
#FenixRoot QPushButton#FenixChip {
    border-radius: 17px; padding: 0 16px; background: rgba(255, 255, 255, 12);
    border: 1px solid rgba(255, 255, 255, 22); color: #c9c9d4; min-height: 32px; max-height: 32px;
}
#FenixRoot QPushButton#FenixChip:hover { background: rgba(255, 255, 255, 28); color: white; }
#FenixRoot QPushButton#FenixChip:checked { background: #f4f4f6; border-color: #f4f4f6; color: #101014; }
#FenixRoot QComboBox {
    background: rgba(255, 255, 255, 14); color: #f4f4f6; border: 1px solid rgba(255, 255, 255, 26);
    border-radius: 17px; padding: 0 14px; font-size: 13px; font-weight: 600;
    min-height: 32px; max-height: 32px;
}
#FenixRoot QComboBox:hover { background: rgba(255, 255, 255, 28); }
#FenixRoot QComboBox:focus { border: 1px solid #ff2d3d; }
#FenixRoot QComboBox::drop-down { border: none; width: 28px; }
#FenixRoot QComboBox QAbstractItemView {
    background: #1f1f27; color: #f4f4f6; selection-background-color: #e60012; border: 1px solid #33333f;
    padding: 4px; outline: none;
}
#FenixRoot QListView { background: transparent; border: none; outline: none; }
#FenixRoot QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }
#FenixRoot QScrollBar::handle:vertical { background: rgba(255, 255, 255, 50); border-radius: 3px; min-height: 40px; }
#FenixRoot QScrollBar::handle:vertical:hover { background: #e60012; }
#FenixRoot QScrollBar::add-line, #FenixRoot QScrollBar::sub-line { height: 0; width: 0; }
#FenixRoot QScrollBar::add-page, #FenixRoot QScrollBar::sub-page { background: transparent; }
#FenixRoot QSlider::groove:horizontal { height: 4px; background: rgba(255, 255, 255, 40); border-radius: 2px; }
#FenixRoot QSlider::sub-page:horizontal { background: #e60012; border-radius: 2px; }
#FenixRoot QSlider::handle:horizontal {
    width: 14px; height: 14px; margin: -6px 0; border-radius: 8px; background: #ffffff;
    border: 1px solid #ffffff;
}
#FenixRoot QCheckBox { color: #f4f4f6; }
#FenixRoot QListWidget { background: #1b1b22; color: #f4f4f6; border: 1px solid #33333f; border-radius: 10px; }
#FenixRoot QListWidget::item { padding: 8px; }
#FenixRoot QListWidget::item:selected { background: #e60012; color: white; }
)");
}

} // namespace Fenix::Theme
