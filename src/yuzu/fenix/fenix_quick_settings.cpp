// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <thread>

#include <QAbstractButton>
#include <QTabBar>
#include <QRadioButton>
#include <QComboBox>
#include <QCheckBox>
#include <QAbstractSpinBox>
#include <QAbstractSlider>
#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QListWidget>
#include <QFileInfo>
#include <QDir>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

#include "common/literals.h"
#include "common/memory_detect.h"
#include "common/recommended_settings.h"
#include "common/settings.h"
#include "yuzu/configuration/nvidia_dlss_panel.h"
#include "yuzu/fenix/fenix_feedback.h"
#include "yuzu/fenix/fenix_home.h"
#include "yuzu/fenix/fenix_icons.h"
#include "yuzu/fenix/fenix_library.h"
#include "yuzu/fenix/fenix_quick_settings.h"
#include "yuzu/fenix/fenix_theme.h"

namespace Fenix {

namespace {

/// Sets a setting for good: the saved (global) value, and the running game's value when a
/// per-game value stands in for it, so a change shows in game and survives a restart.
template <typename Setting, typename Value>
void Set(Setting& setting, const Value& value) {
    const bool using_global = setting.UsingGlobal();
    setting.SetGlobal(true);
    setting.SetValue(value);
    setting.SetGlobal(using_global);
    if (!using_global) {
        setting.SetValue(value);
    }
}

using Settings::ResolutionSetup;

int ResolutionIndex() {
    switch (Settings::values.resolution_setup.GetValue()) {
    case ResolutionSetup::Res1X:
        return 0;
    case ResolutionSetup::Res2X:
        return 1;
    case ResolutionSetup::Res3X:
        return 2;
    case ResolutionSetup::Res4X:
        return 3;
    default:
        return -1;
    }
}

void SetResolution(int index) {
    static constexpr ResolutionSetup values[] = {ResolutionSetup::Res1X, ResolutionSetup::Res2X,
                                                 ResolutionSetup::Res3X, ResolutionSetup::Res4X};
    Set(Settings::values.resolution_setup, values[std::clamp(index, 0, 3)]);
}

} // namespace

u16 QuickSettings::OcarinaFrameRate() {
    static constexpr u16 rates[] = {60, 90, 120, 144};
    return rates[std::clamp(Library::Get().Option(QStringLiteral("oot_fps"), 3), 0, 3)];
}

void QuickSettings::ApplyStartupOptions() {
    // Every choice here is a regular setting: nothing to pass on at startup.
}

QuickSettings::QuickSettings(QWidget* parent) : QDialog(parent) {
    setObjectName(QStringLiteral("FenixRoot"));
    setStyleSheet(Theme::StyleSheet() + QStringLiteral(R"(
QDialog#FenixRoot { background: #121216; }
QLabel#FenixRowTitle { color: #ffffff; font-size: 15px; font-weight: 700; }
QLabel#FenixRowHint { color: #9a9aa8; font-size: 12px; }
QFrame#FenixRow { background: rgba(255, 255, 255, 10); border: 1px solid rgba(255, 255, 255, 16); border-radius: 14px; }
QFrame#FenixSegment { background: rgba(0, 0, 0, 90); border: 1px solid rgba(255, 255, 255, 20); border-radius: 19px; }
QPushButton#FenixOption {
    border: 1px solid transparent; background: transparent; border-radius: 15px; padding: 0 14px;
    min-height: 30px; max-height: 30px; min-width: 44px; color: #c9c9d4;
}
QPushButton#FenixOption:hover { background: rgba(255, 255, 255, 26); color: white; }
QPushButton#FenixOption:focus {
    border: 1px solid #ffffff; padding: 0 14px; min-height: 30px; max-height: 30px;
}
QPushButton#FenixOption:checked { background: #e60012; color: white; }
QPushButton#FenixPreset {
    border-radius: 14px; padding: 0; min-height: 96px; max-height: 96px; background: rgba(255, 255, 255, 10);
    border: 1px solid rgba(255, 255, 255, 22);
}
QPushButton#FenixPreset:hover { border-color: #ff2d3d; background: rgba(230, 0, 18, 40); }
QPushButton#FenixPreset:focus {
    border: 2px solid #ff2d3d; padding: 0; min-height: 94px; max-height: 94px;
}
QLabel#FenixPresetName { color: #ffffff; font-size: 15px; font-weight: 800; }
QLabel#FenixPresetHint { color: #a9a9b6; font-size: 11px; }
QLabel#FenixBadge {
    background: #e60012; color: white; font-size: 9px; font-weight: 800; letter-spacing: 1px;
    border-radius: 8px; padding: 0 7px; min-height: 16px; max-height: 16px;
}
QPushButton#FenixPreset:checked { border: 2px solid #ff2d3d; background: rgba(230, 0, 18, 56); min-height: 94px; max-height: 94px; }
QPushButton#FenixAdvanced {
    background: transparent; color: #c9c9d6; font-size: 13px; font-weight: 700; border: none;
    padding: 4px 2px; text-align: left;
}
QPushButton#FenixAdvanced:hover, QPushButton#FenixAdvanced:focus { color: #ffffff; }
QFrame#FenixBest {
    background: rgba(230, 0, 18, 28); border: 1px solid rgba(255, 45, 61, 120); border-radius: 14px;
}
QPushButton#FenixBestButton {
    background: #e60012; color: white; font-size: 14px; font-weight: 800; border: none;
    border-radius: 19px; padding: 0 22px; min-height: 38px; max-height: 38px;
}
QPushButton#FenixBestButton:hover { background: #ff2d3d; }
QPushButton#FenixBestButton:focus { border: 2px solid white; padding: 0 20px; min-height: 34px; max-height: 34px; }
QPushButton#FenixBestButton:disabled {
    background: rgba(60, 200, 120, 40); color: #7be3a6; border: 1px solid rgba(123, 227, 166, 120);
}
QScrollArea { background: transparent; border: none; }
QWidget#FenixScroll { background: transparent; }
)"));
    setWindowTitle(tr("Quick Settings"));
    resize(860, 840);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(26, 22, 26, 18);
    outer->setSpacing(12);

    auto* title = new QLabel(tr("Quick Settings"), this);
    title->setObjectName(QStringLiteral("FenixEmptyTitle"));
    outer->addWidget(title);
    auto* subtitle = new QLabel(tr("Pick a mode. That's all most games need."), this);
    subtitle->setObjectName(QStringLiteral("FenixMuted"));
    outer->addWidget(subtitle);

    // Modes: one tap sets everything. The single settings are under Advanced.
    auto* modes = new QHBoxLayout;
    modes->setSpacing(10);
    const QStringList mode_names = ModeNames();
    const QStringList mode_hints{
        tr("The best picture and speed for your PC. Start here."),
        tr("Best, plus frame generation (AMD FSR 3, 2x) for smoother motion."),
        tr("Best at 2x resolution. For Switch 1 games: Switch 2 games already render sharp.")};
    for (int i = 0; i < mode_names.size(); ++i) {
        auto* button = new QPushButton(this);
        button->setObjectName(QStringLiteral("FenixPreset"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setAccessibleName(mode_names[i]);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* inner = new QVBoxLayout(button);
        inner->setContentsMargins(14, 10, 14, 10);
        inner->setSpacing(3);
        auto* name = new QLabel(mode_names[i], button);
        name->setObjectName(QStringLiteral("FenixPresetName"));
        auto* hint = new QLabel(mode_hints[i], button);
        hint->setObjectName(QStringLiteral("FenixPresetHint"));
        hint->setWordWrap(true);
        inner->addStretch();
        if (i == 0) {
            auto* badge = new QLabel(tr("RECOMMENDED"), button);
            badge->setObjectName(QStringLiteral("FenixBadge"));
            inner->addWidget(badge, 0, Qt::AlignLeft);
            inner->addSpacing(2);
        }
        inner->addWidget(name);
        inner->addWidget(hint);
        inner->addStretch();
        for (auto* label : button->findChildren<QLabel*>()) {
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        button->installEventFilter(this);
        connect(button, &QPushButton::clicked, this, [this, i] { ApplyMode(i); });
        modes->addWidget(button);
        mode_buttons.push_back(button);
    }
    outer->addLayout(modes);

    mode_note = new QLabel(this);
    mode_note->setObjectName(QStringLiteral("FenixMuted"));
    mode_note->setWordWrap(true);
    outer->addWidget(mode_note);

    // DLSS up front: the best picture on RTX cards, and it needs one download.
    AddRow(outer, tr("NVIDIA DLSS"),
           tr("RTX cards, Switch 2 games: NVIDIA's AI anti-aliasing and upscaling, sharper than "
              "the game's own. Needs NVIDIA's files once (button below). Applies the next time a "
              "game starts."),
           {tr("Off"), tr("On")},
           [] { return Settings::values.nvidia_dlss.GetValue() ? 1 : 0; },
           [](int index) { Set(Settings::values.nvidia_dlss, index == 1); });
#ifdef EGLANTINE_STREAMLINE
    outer->addWidget(new NvidiaDlssPanel(this));
#endif

    // Volume here instead of the status bar.
    static constexpr std::array<u8, 6> volumes{0, 25, 50, 75, 100, 150};
    AddRow(outer, tr("Volume"), tr("Game sound. Changes right away."),
           {tr("Off"), QStringLiteral("25%"), QStringLiteral("50%"), QStringLiteral("75%"),
            QStringLiteral("100%"), QStringLiteral("150%")},
           [] {
               if (Settings::values.audio_muted) {
                   return 0;
               }
               const auto it = std::ranges::find(volumes, Settings::values.volume.GetValue());
               return it != volumes.end() ? static_cast<int>(it - volumes.begin()) : -1;
           },
           [](int index) {
               Settings::values.audio_muted = false;
               Set(Settings::values.volume, volumes[static_cast<size_t>(index)]);
           });

    advanced_button = new QPushButton(this);
    advanced_button->setObjectName(QStringLiteral("FenixAdvanced"));
    advanced_button->setCursor(Qt::PointingHandCursor);
    advanced_button->installEventFilter(this);
    outer->addWidget(advanced_button, 0, Qt::AlignLeft);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("FenixScroll"));
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 4, 8, 4);
    layout->setSpacing(8);
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
    advanced_area = scroll;
    connect(advanced_button, &QPushButton::clicked, this, [this] {
        Library::Get().SetOption(QStringLiteral("qs_advanced"), advanced_area->isVisible() ? 0 : 1);
        ShowAdvanced(!advanced_area->isVisible());
    });

    const auto option_row = [this, layout](const QString& title, const QString& hint,
                                           const QStringList& options, const char* name) {
        const QString key = QString::fromLatin1(name);
        AddRow(layout, title, hint, options,
               [key, count = static_cast<int>(options.size())] {
                   return std::clamp(Library::Get().Option(key), 0, count - 1);
               },
               [key](int index) { Library::Get().SetOption(key, index); });
    };

    AddRow(layout, tr("Ocarina of Time frame rate"),
           tr("Ocarina of Time has a built-in FPS unlock, so it really runs faster above 60 "
              "(game speed stays right). 144 by default. Other games stay at their own 60: "
              "they would run too fast. Applies the next time the game starts."),
           {tr("60"), tr("90"), tr("120"), tr("144")},
           [] { return std::clamp(Library::Get().Option(QStringLiteral("oot_fps"), 3), 0, 3); },
           [](int index) { Library::Get().SetOption(QStringLiteral("oot_fps"), index); });

    option_row(tr("Controller"),
               tr("Automatic: a controller you plug in or pair (8BitDo, Xbox, PlayStation, Pro "
                  "Controller) becomes player 1 by itself. Manual: only what you set up in All "
                  "Settings > Controls. Changes right away."),
               {tr("Automatic"), tr("Manual")}, "manual_controller");

    AddRow(layout, tr("Resolution"),
           tr("Switch games: 1x is 1080p docked, 2x is 4K. Keep 1x for Switch 2 games: they "
              "already render sharp, and higher replaces their own upscaler. Applies the next "
              "time a game starts."),
           {tr("1x"), tr("2x (4K)"), tr("3x"), tr("4x")}, ResolutionIndex, SetResolution);

    AddRow(layout, tr("Frame generation"),
           tr("Extra in-between frames for smoother motion (AMD FSR 3). Best when the game "
              "already runs near full speed. Changes right away."),
           {tr("Off"), tr("2x"), tr("3x"), tr("4x")},
           [] {
               if (!Settings::values.frame_gen.GetValue()) {
                   return 0;
               }
               return std::clamp<int>(
                   static_cast<int>(Settings::values.frame_gen_multiplier.GetValue()) - 1, 1, 3);
           },
           [](int index) {
               Set(Settings::values.frame_gen, index > 0);
               if (index > 0) {
                   Set(Settings::values.frame_gen_type, Settings::FrameGenType::Fsr3);
                   Set(Settings::values.frame_gen_multiplier, static_cast<u32>(index + 1));
               }
           });

    AddRow(layout, tr("Ocarina of Time sharpening"),
           tr("The game's own anti-aliasing sharpens the image. No sharpening: softer, less "
              "shimmer. Changes right away."),
           {tr("Game"), tr("No sharpening")},
           [] { return static_cast<int>(Settings::values.oot_anti_aliasing.GetValue()); },
           [](int index) {
               Set(Settings::values.oot_anti_aliasing, 
                   static_cast<Settings::OotAntiAliasing>(index));
           });

    AddRow(layout, tr("VSync"),
           tr("On: no tearing (FIFO Relaxed). Off: lowest input lag, may tear (Mailbox). "
              "Changes right away."),
           {tr("On"), tr("Off")},
           [] {
               const auto mode = Settings::values.vsync_mode.GetValue();
               return mode == Settings::VSyncMode::Fifo || mode == Settings::VSyncMode::FifoRelaxed
                          ? 0
                          : 1;
           },
           [](int index) {
               Set(Settings::values.vsync_mode, index == 0 ? Settings::VSyncMode::FifoRelaxed
                                                               : Settings::VSyncMode::Mailbox);
           });

    AddRow(layout, tr("Console mode"),
           tr("Docked renders at TV resolution (higher), handheld at the portable one. Changes "
              "right away."),
           {tr("Docked (TV)"), tr("Handheld")},
           [] {
               return Settings::values.use_docked_mode.GetValue() == Settings::ConsoleMode::Docked
                          ? 0
                          : 1;
           },
           [](int index) {
               Set(Settings::values.use_docked_mode, index == 0 ? Settings::ConsoleMode::Docked
                                                                    : Settings::ConsoleMode::Handheld);
           });

    AddRow(layout, tr("Anti-aliasing"), tr("Smooths jagged edges. Changes right away."),
           {tr("Off"), tr("FXAA"), tr("SMAA")},
           [] { return static_cast<int>(Settings::values.anti_aliasing.GetValue()); },
           [](int index) {
               Set(Settings::values.anti_aliasing, static_cast<Settings::AntiAliasing>(index));
           });

#if defined(_WIN32) || defined(__linux__)
    AddRow(layout, tr("Pin GPU threads"),
           tr("Keeps the graphics threads on their own CPU cores. Recommended. Applies the next "
              "time a game starts."),
           {tr("On"), tr("Off")},
           [] { return Settings::values.pin_gpu_threads.GetValue() ? 0 : 1; },
           [](int index) { Set(Settings::values.pin_gpu_threads, index == 0); });
#endif

    AddRow(layout, tr("Present on its own thread"),
           tr("Recommended: the emulator's GPU thread doesn't wait for your monitor every frame. "
              "Applies the next time a game starts."),
           {tr("On"), tr("Off")},
           [] { return Settings::values.async_presentation.GetValue() ? 0 : 1; },
           [](int index) { Set(Settings::values.async_presentation, index == 0); });
    AddRow(layout, tr("Prepare shaders before playing"),
           tr("Builds a game's shaders on the loading screen, so new areas don't stutter the "
              "first time you see them. The first time takes a few minutes (it has a Skip "
              "button); later only what's new. Applies the next time a game starts."),
           {tr("Always"), tr("Ask"), tr("Off")},
           [] {
               switch (Settings::values.precache_shaders.GetValue()) {
               case Settings::PrecacheShaders::Always:
                   return 0;
               case Settings::PrecacheShaders::Ask:
                   return 1;
               default:
                   return 2;
               }
           },
           [](int index) {
               Set(Settings::values.precache_shaders,
                   index == 0   ? Settings::PrecacheShaders::Always
                   : index == 1 ? Settings::PrecacheShaders::Ask
                                : Settings::PrecacheShaders::Off);
           });
    AddRow(layout, tr("Menu sounds"), tr("Soft sounds when you move around Fenix. Changes right away."),
           {tr("On"), tr("Off")},
           [] { return Library::Get().Option(QStringLiteral("ui_sounds"), 1) != 0 ? 0 : 1; },
           [](int index) { Library::Get().SetOption(QStringLiteral("ui_sounds"), index == 0); });
    AddRow(layout, tr("Controller rumble in menus"),
           tr("A light buzz when you move around Fenix with a controller. Changes right away."),
           {tr("On"), tr("Off")},
           [] { return Library::Get().Option(QStringLiteral("ui_rumble"), 1) != 0 ? 0 : 1; },
           [](int index) { Library::Get().SetOption(QStringLiteral("ui_rumble"), index == 0); });

    layout->addStretch();
    UpdateModes();

    auto* bottom = new QHBoxLayout;
    status = new QLabel(this);
    status->setObjectName(QStringLiteral("FenixMuted"));
    bottom->addWidget(status, 1);
    close_game_button = new QPushButton(tr("Close Game"), this);
    close_game_button->installEventFilter(this);
    close_game_button->setVisible(false);
    connect(close_game_button, &QPushButton::clicked, this, [this] {
        close_game = true;
        accept();
    });
    backup_button = new QPushButton(tr("Back Up Saves"), this);
    backup_button->installEventFilter(this);
    backup_button->setVisible(false);
    backup_button->setToolTip(tr("Copies this game's saves now. Put a backup back from the "
                                 "game's menu in the library (Restore a Backup)."));
    connect(backup_button, &QPushButton::clicked, this, [this] {
        status->setText(Home::BackupSavesNow(program_id));
    });
    bottom->addWidget(backup_button);
    bottom->addWidget(close_game_button);
    auto* all = new QPushButton(tr("All Settings..."), this);
    all->installEventFilter(this);
    connect(all, &QPushButton::clicked, this, [this] {
        accept();
        emit OpenAllSettings();
    });
    auto* done = new QPushButton(tr("Done"), this);
    done->setObjectName(QStringLiteral("FenixPrimary"));
    done->installEventFilter(this);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(all);
    bottom->addWidget(done);
    outer->addLayout(bottom);

    RefreshRows();
    done->setFocus();
    ShowAdvanced(Library::Get().Option(QStringLiteral("qs_advanced")) == 1);
}

void QuickSettings::AddRow(QVBoxLayout* layout, const QString& title, const QString& hint,
                           const QStringList& options, std::function<int()> current,
                           std::function<void(int)> apply) {
    // One card per setting: what it is on the left, its choices as a segmented control on the
    // right.
    auto* box = new QFrame(layout->parentWidget());
    box->setObjectName(QStringLiteral("FenixRow"));
    auto* box_layout = new QHBoxLayout(box);
    box_layout->setContentsMargins(16, 12, 12, 12);
    box_layout->setSpacing(16);
    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* label = new QLabel(title, box);
    label->setObjectName(QStringLiteral("FenixRowTitle"));
    auto* hint_label = new QLabel(hint, box);
    hint_label->setObjectName(QStringLiteral("FenixRowHint"));
    hint_label->setWordWrap(true);
    text->addWidget(label);
    text->addWidget(hint_label);
    box_layout->addLayout(text, 1);

    auto* segment = new QFrame(box);
    segment->setObjectName(QStringLiteral("FenixSegment"));
    auto* buttons = new QHBoxLayout(segment);
    buttons->setContentsMargins(3, 3, 3, 3);
    buttons->setSpacing(2);
    auto* group = new QButtonGroup(box);
    group->setExclusive(true);
    for (int i = 0; i < options.size(); ++i) {
        auto* button = new QPushButton(options[i], segment);
        button->setObjectName(QStringLiteral("FenixOption"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->installEventFilter(this);
        group->addButton(button, i);
        buttons->addWidget(button);
    }
    // Rows that say so apply when the game starts again: MainWindow offers a restart.
    const bool needs_restart = hint.contains(QStringLiteral("next time"));
    connect(group, &QButtonGroup::idClicked, this, [this, apply, needs_restart](int id) {
        apply(id);
        if (needs_restart) {
            restart_needed = true;
        }
        Changed();
    });
    box_layout->addWidget(segment, 0, Qt::AlignVCenter | Qt::AlignRight);
    layout->addWidget(box);
    rows.push_back({group, std::move(current)});
}

void QuickSettings::SetInGame(bool in_game, u64 game_program_id) {
    program_id = game_program_id;
    close_game_button->setVisible(in_game);
    backup_button->setVisible(in_game && game_program_id != 0);
    if (in_game && status != nullptr) {
        status->setText(tr("Game paused. Each setting says when it applies. B / Esc to go back."));
    }
}

void QuickSettings::RefreshRows() {
    for (auto& row : rows) {
        const int index = row.current();
        if (auto* button = row.group->button(index)) {
            button->setChecked(true);
        } else {
            // Not one of the choices here (set in All Settings): show none selected.
            row.group->setExclusive(false);
            for (auto* other : row.group->buttons()) {
                other->setChecked(false);
            }
            row.group->setExclusive(true);
        }
    }
}

namespace {
enum Mode { Best, FrameGen, FourK, ModeCount };

/// What a mode sets on top of the recommended settings.
struct ModeChoices {
    int resolution; ///< Index for SetResolution
    Settings::AntiAliasing anti_aliasing;
    bool frame_gen;
};

ModeChoices ChoicesOf(int mode) {
    switch (mode) {
    case FrameGen:
        return {0, Settings::AntiAliasing::Fxaa, true};
    case FourK:
        return {1, Settings::AntiAliasing::Smaa, false};
    default:
        return {0, Settings::AntiAliasing::Fxaa, false};
    }
}
} // namespace

bool QuickSettings::RouteControllerKey(Qt::Key key) {
    // A menu or a dialog that is open takes the controller.
    QWidget* target = QApplication::activePopupWidget();
    if (target == nullptr) {
        QWidget* const modal = QApplication::activeModalWidget();
        if (modal == nullptr) {
            return false;
        }
        QWidget* const focus = QApplication::focusWidget();
        target = focus != nullptr && modal->isAncestorOf(focus) ? focus : modal;
        // Other dialogs (questions, All Settings): the D-pad moves from control to control
        // (lists, sliders, number boxes and tabs keep their own arrows), A presses or opens, L
        // and R switch tabs.
        if (modal->objectName() != QStringLiteral("FenixRoot")) {
            const bool own_arrows = qobject_cast<QAbstractItemView*>(target) != nullptr ||
                                    qobject_cast<QAbstractSlider*>(target) != nullptr ||
                                    qobject_cast<QAbstractSpinBox*>(target) != nullptr ||
                                    qobject_cast<QTabBar*>(target) != nullptr;
            Qt::KeyboardModifiers modifiers = Qt::NoModifier;
            if (!own_arrows && (key == Qt::Key_Left || key == Qt::Key_Up)) {
                key = Qt::Key_Backtab;
            } else if (!own_arrows && (key == Qt::Key_Right || key == Qt::Key_Down)) {
                key = Qt::Key_Tab;
            } else if ((key == Qt::Key_Enter || key == Qt::Key_Return) &&
                       (qobject_cast<QCheckBox*>(target) != nullptr ||
                        qobject_cast<QRadioButton*>(target) != nullptr ||
                        qobject_cast<QComboBox*>(target) != nullptr)) {
                key = Qt::Key_Space;
            } else if (key == Qt::Key_PageUp || key == Qt::Key_PageDown) {
                modifiers = Qt::ControlModifier;
                key = key == Qt::Key_PageDown ? Qt::Key_Tab : Qt::Key_Backtab;
            }
            QCoreApplication::postEvent(target, new QKeyEvent(QEvent::KeyPress, key, modifiers));
            return true;
        }
    }
    QCoreApplication::postEvent(target, new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier));
    return true;
}

QStringList QuickSettings::ModeNames() {
    return {tr("Best"), tr("Frame Generation"), tr("4K")};
}

int QuickSettings::CurrentMode() {
    const auto& v = Settings::values;
    const auto vsync = v.vsync_mode.GetValue();
    if (!v.async_presentation.GetValue() || !v.use_disk_shader_cache.GetValue() ||
        v.astc_recompression.GetValue() != Settings::AstcRecompression::Bc3 ||
        !(vsync == Settings::VSyncMode::Fifo || vsync == Settings::VSyncMode::FifoRelaxed) ||
        v.use_docked_mode.GetValue() != Settings::ConsoleMode::Docked) {
        return -1;
    }
    if (Library::Get().Option(QStringLiteral("oot_fps"), 3) != 3 ||
        v.oot_anti_aliasing.GetValue() != Settings::OotAntiAliasing::Game) {
        return -1;
    }
    const bool fast = v.gpu_accuracy.GetValue() == Settings::GpuAccuracy::Low &&
                      v.use_asynchronous_shaders.GetValue();
    for (int mode = 0; mode < ModeCount; ++mode) {
        const ModeChoices choices = ChoicesOf(mode);
        if (fast && ResolutionIndex() == choices.resolution &&
            v.anti_aliasing.GetValue() == choices.anti_aliasing &&
            v.frame_gen.GetValue() == choices.frame_gen) {
            return mode;
        }
    }
    return -1;
}

void QuickSettings::ApplyMode(int mode) {
    const ModeChoices choices = ChoicesOf(mode);
    auto& v = Settings::values;
    // The list of what runs Switch 2 games best (Emulation > Apply Recommended Settings):
    // accuracy, shader and texture handling, threads, presentation.
    for (const auto& change : Settings::ApplyRecommendedSettings(v)) {
        // It sets the saved values; a running game's per-game value follows them too.
        auto* setting = const_cast<Settings::BasicSetting*>(change.setting);
        if (setting->Switchable() && !setting->UsingGlobal()) {
            setting->LoadString(change.current);
        }
    }
    Library::Get().SetOption(QStringLiteral("oot_fps"), 3); // 144: the fastest Ocarina runs
    Set(v.vsync_mode, Settings::VSyncMode::FifoRelaxed);
    Set(v.use_docked_mode, Settings::ConsoleMode::Docked);
    SetResolution(choices.resolution);
    Set(v.anti_aliasing, choices.anti_aliasing);
    Set(v.oot_anti_aliasing, Settings::OotAntiAliasing::Game);
    Set(v.frame_gen, choices.frame_gen);
    Set(v.precache_shaders, Settings::PrecacheShaders::Always);
    if (choices.frame_gen) {
        Set(v.frame_gen_type, Settings::FrameGenType::Fsr3);
        Set(v.frame_gen_multiplier, 2U);
    }
    restart_needed = true;
    Changed();
    status->setText(tr("%1 on. Start the game again for all of it to apply.")
                        .arg(ModeNames().value(mode)));
}

void QuickSettings::UpdateModes() {
    const int mode = CurrentMode();
    for (int i = 0; i < static_cast<int>(mode_buttons.size()); ++i) {
        mode_buttons[static_cast<size_t>(i)]->setChecked(i == mode);
    }
    if (mode_note != nullptr) {
        mode_note->setText(mode >= 0 ? QString{}
                                     : tr("Custom: something under Advanced differs from the "
                                          "modes. Tap a mode to go back."));
        mode_note->setVisible(mode < 0);
    }
}

void QuickSettings::ShowAdvanced(bool show) {
    advanced_area->setVisible(show);
    advanced_button->setText(show ? tr("Advanced  \u25B4") : tr("Advanced (every setting)  \u25BE"));
    resize(width(), show ? 900 : 540);
}

void QuickSettings::Changed() {
    Settings::UpdateGPUAccuracy();
    RefreshRows();
    UpdateModes();
    emit SettingsChanged();
    if (status != nullptr) {
        status->setText(tr("Saved. Some changes apply the next time a game starts."));
    }
}

bool QuickSettings::eventFilter(QObject* object, QEvent* event) {
    // Controller and keyboard: arrows move between buttons, Enter presses, Escape closes.
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_Right:
        case Qt::Key_Down:
            focusNextChild();
            return true;
        case Qt::Key_Left:
        case Qt::Key_Up:
            focusPreviousChild();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (auto* button = qobject_cast<QAbstractButton*>(object)) {
                button->click();
                return true;
            }
            break;
        case Qt::Key_Escape:
            reject();
            return true;
        default:
            break;
        }
    }
    return QDialog::eventFilter(object, event);
}

namespace {
/// The leave dialog: arrows (and a controller's D-pad) move between the choices.
class LeaveGameDialog final : public QDialog {
public:
    using QDialog::QDialog;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        switch (event->key()) {
        case Qt::Key_Down:
        case Qt::Key_Right:
            focusNextChild();
            return;
        case Qt::Key_Up:
        case Qt::Key_Left:
            focusPreviousChild();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (auto* button = qobject_cast<QPushButton*>(focusWidget())) {
                button->click();
                return;
            }
            break;
        default:
            break;
        }
        QDialog::keyPressEvent(event);
    }
};
} // namespace

LeaveChoice AskLeaveGame(QWidget* parent, const QString& game_title) {
    LeaveGameDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("FenixRoot"));
    dialog.setWindowTitle(QObject::tr("Leave the game?"));
    dialog.setStyleSheet(Theme::StyleSheet() + QStringLiteral(R"(
QDialog#FenixRoot { background: #121216; }
QLabel#FenixLeaveTitle { color: #ffffff; font-size: 24px; font-weight: 800; }
QPushButton#FenixLeave {
    text-align: left; border-radius: 14px; padding: 0; min-height: 64px; max-height: 64px;
    background: rgba(255, 255, 255, 10); border: 1px solid rgba(255, 255, 255, 22);
}
QPushButton#FenixLeave:hover { background: rgba(255, 255, 255, 22); }
QPushButton#FenixLeave:focus { border: 2px solid #ff2d3d; background: rgba(230, 0, 18, 46); }
QLabel#FenixLeaveName { color: #ffffff; font-size: 15px; font-weight: 800; }
QLabel#FenixLeaveHint { color: #a9a9b6; font-size: 12px; }
)"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(26, 22, 26, 22);
    layout->setSpacing(10);
    auto* title = new QLabel(QObject::tr("Leave the game?"), &dialog);
    title->setObjectName(QStringLiteral("FenixLeaveTitle"));
    layout->addWidget(title);
    auto* subtitle = new QLabel(
        game_title.isEmpty()
            ? QObject::tr("Anything since your last save is lost.")
            : QObject::tr("%1\nAnything since your last save is lost.").arg(game_title),
        &dialog);
    subtitle->setObjectName(QStringLiteral("FenixMuted"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);
    layout->addSpacing(6);

    LeaveChoice choice = LeaveChoice::KeepPlaying;
    const auto add = [&](const QString& name, const QString& hint, LeaveChoice value,
                         const QColor& accent) {
        auto* button = new QPushButton(&dialog);
        button->setObjectName(QStringLiteral("FenixLeave"));
        button->setCursor(Qt::PointingHandCursor);
        button->setAutoDefault(false);
        auto* row = new QHBoxLayout(button);
        row->setContentsMargins(16, 8, 16, 8);
        row->setSpacing(14);
        auto* dot = new QLabel(button);
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 5px;").arg(accent.name()));
        row->addWidget(dot, 0, Qt::AlignVCenter);
        auto* text = new QVBoxLayout;
        text->setSpacing(2);
        auto* name_label = new QLabel(name, button);
        name_label->setObjectName(QStringLiteral("FenixLeaveName"));
        auto* hint_label = new QLabel(hint, button);
        hint_label->setObjectName(QStringLiteral("FenixLeaveHint"));
        text->addWidget(name_label);
        text->addWidget(hint_label);
        row->addLayout(text, 1);
        for (auto* label : button->findChildren<QLabel*>()) {
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        QObject::connect(button, &QPushButton::clicked, &dialog, [&dialog, &choice, value] {
            choice = value;
            dialog.accept();
        });
        layout->addWidget(button);
        return button;
    };
    QPushButton* close_game =
        add(QObject::tr("Close Game"), QObject::tr("Back to your library. Fenix stays open."),
            LeaveChoice::CloseGame, Theme::Red);
    add(QObject::tr("Quit Fenix"), QObject::tr("Close the game and Fenix."), LeaveChoice::QuitApp,
        Theme::TextMuted);
    add(QObject::tr("Keep Playing"), QObject::tr("Back to the game (B / Esc)."),
        LeaveChoice::KeepPlaying, QColor(0x4c, 0xd9, 0x7b));
    dialog.resize(460, dialog.sizeHint().height());
    close_game->setFocus();
    dialog.exec();
    return choice;
}

namespace {
/// The folder list of PickFolder: Enter opens, Escape goes up, X / Y adds the folder shown.
class FolderPickerDialog final : public QDialog {
public:
    explicit FolderPickerDialog(QWidget* parent) : QDialog(parent) {}

    std::function<void()> on_open;
    std::function<void()> on_up;
    std::function<void()> on_add;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (qobject_cast<QListWidget*>(focusWidget()) != nullptr) {
                on_open();
                return;
            }
            if (auto* button = qobject_cast<QPushButton*>(focusWidget())) {
                button->click();
                return;
            }
            break;
        case Qt::Key_Escape:
        case Qt::Key_Backspace:
            on_up();
            return;
        case Qt::Key_F: // X on a controller
        case Qt::Key_S: // Y on a controller
            on_add();
            return;
        case Qt::Key_Left:
        case Qt::Key_Right:
            if (event->key() == Qt::Key_Right) {
                focusNextChild();
            } else {
                focusPreviousChild();
            }
            return;
        default:
            break;
        }
        QDialog::keyPressEvent(event);
    }
};
} // namespace

QString PickFolder(QWidget* parent, const QString& start) {
    FolderPickerDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("FenixRoot"));
    dialog.setWindowTitle(QObject::tr("Add Games"));
    dialog.setStyleSheet(Theme::StyleSheet() + QStringLiteral(R"(
QDialog#FenixRoot { background: #121216; }
QLabel#FenixLeaveTitle { color: #ffffff; font-size: 24px; font-weight: 800; }
QListWidget { background: rgba(255, 255, 255, 8); border: 1px solid rgba(255, 255, 255, 20);
    border-radius: 14px; padding: 6px; color: #f4f4f6; font-size: 15px; outline: none; }
QListWidget::item { padding: 10px 12px; border-radius: 10px; }
QListWidget::item:selected { background: #e60012; color: white; }
QListWidget::item:hover:!selected { background: rgba(255, 255, 255, 18); }
)"));
    dialog.resize(640, 620);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(10);
    auto* title = new QLabel(QObject::tr("Add Games"), &dialog);
    title->setObjectName(QStringLiteral("FenixLeaveTitle"));
    layout->addWidget(title);
    auto* where = new QLabel(&dialog);
    where->setObjectName(QStringLiteral("FenixMuted"));
    where->setWordWrap(true);
    layout->addWidget(where);
    auto* list = new QListWidget(&dialog);
    list->setIconSize(QSize(22, 22));
    layout->addWidget(list, 1);
    auto* hint = new QLabel(QObject::tr("A  Open folder     B  Back     Y  Add this folder"),
                            &dialog);
    hint->setObjectName(QStringLiteral("FenixMuted"));
    layout->addWidget(hint);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = new QPushButton(QObject::tr("Cancel"), &dialog);
    auto* add = new QPushButton(QObject::tr("Add This Folder"), &dialog);
    add->setObjectName(QStringLiteral("FenixPrimary"));
    buttons->addWidget(cancel);
    buttons->addWidget(add);
    layout->addLayout(buttons);

    QString current; // empty: the list of drives
    QString chosen;
    const QIcon folder_icon = Icons::Icon(Icons::Glyph::Folder, Theme::Text, 22);
    std::function<void(const QString&, const QString&)> show;
    show = [&](const QString& path, const QString& select) {
        current = path;
        list->clear();
        if (path.isEmpty()) {
            where->setText(QObject::tr("Choose a drive"));
            for (const QFileInfo& drive : QDir::drives()) {
                auto* item = new QListWidgetItem(folder_icon, QDir::toNativeSeparators(
                                                                  drive.absoluteFilePath()));
                item->setData(Qt::UserRole, drive.absoluteFilePath());
                list->addItem(item);
            }
        } else {
            where->setText(QObject::tr("In %1  ·  Y adds this folder and the games in it")
                               .arg(QDir::toNativeSeparators(path)));
            const QDir dir{path};
            for (const QFileInfo& entry :
                 dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
                                   QDir::Name | QDir::IgnoreCase)) {
                auto* item = new QListWidgetItem(folder_icon, entry.fileName());
                item->setData(Qt::UserRole, entry.absoluteFilePath());
                list->addItem(item);
            }
            if (list->count() == 0) {
                auto* item = new QListWidgetItem(QObject::tr("(no folders inside)"));
                item->setFlags(Qt::NoItemFlags);
                list->addItem(item);
            }
        }
        add->setEnabled(!path.isEmpty());
        int row = 0;
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toString() == select) {
                row = i;
            }
        }
        list->setCurrentRow(row);
        list->setFocus();
    };
    dialog.on_open = [&] {
        if (QListWidgetItem* item = list->currentItem()) {
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty()) {
                Feedback::Play(Feedback::Sound::Select);
                show(path, {});
            }
        }
    };
    dialog.on_up = [&] {
        Feedback::Play(Feedback::Sound::Back);
        if (current.isEmpty()) {
            dialog.reject();
            return;
        }
        QDir dir{current};
        const QString from = dir.absolutePath();
        show(dir.cdUp() ? dir.absolutePath() : QString{}, from);
    };
    dialog.on_add = [&] {
        if (!current.isEmpty()) {
            chosen = current;
            dialog.accept();
        }
    };
    QObject::connect(list, &QListWidget::itemActivated, &dialog,
                     [&](QListWidgetItem*) { dialog.on_open(); });
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(add, &QPushButton::clicked, &dialog, [&] { dialog.on_add(); });

    show(start.isEmpty() || !QDir{start}.exists() ? QString{} : QDir{start}.absolutePath(), {});
    dialog.exec();
    return chosen;
}

} // namespace Fenix
