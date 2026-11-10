// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <thread>

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
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
    const QStringList mode_names{tr("Best"), tr("Smooth"), tr("Sharp"), tr("Safe")};
    const QStringList mode_hints{
        tr("Recommended settings. Start here."),
        tr("Best, plus extra in-between frames for smoother motion."),
        tr("Best, at 2x resolution (4K) with finer edges. Switch 1 games."),
        tr("Slower, most compatible. When a game glitches or crashes.")};
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

    layout->addStretch();
    UpdateModes();

    auto* bottom = new QHBoxLayout;
    status = new QLabel(this);
    status->setObjectName(QStringLiteral("FenixMuted"));
    bottom->addWidget(status, 1);
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

void QuickSettings::SetInGame(bool in_game) {
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
enum Mode { Best, Smooth, Sharp, Safe, ModeCount };

/// What a mode sets on top of the recommended settings.
struct ModeChoices {
    int resolution; ///< Index for SetResolution
    Settings::AntiAliasing anti_aliasing;
    bool frame_gen;
    bool safe; ///< Accurate GPU, draws wait for their shaders
};

ModeChoices ChoicesOf(int mode) {
    switch (mode) {
    case Smooth:
        return {0, Settings::AntiAliasing::Fxaa, true, false};
    case Sharp:
        return {1, Settings::AntiAliasing::Smaa, false, false};
    case Safe:
        return {0, Settings::AntiAliasing::Fxaa, false, true};
    default:
        return {0, Settings::AntiAliasing::Fxaa, false, false};
    }
}
} // namespace

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
    const bool safe = v.gpu_accuracy.GetValue() == Settings::GpuAccuracy::High;
    const bool fast = v.gpu_accuracy.GetValue() == Settings::GpuAccuracy::Low &&
                      v.use_asynchronous_shaders.GetValue();
    for (int mode = 0; mode < ModeCount; ++mode) {
        const ModeChoices choices = ChoicesOf(mode);
        if ((choices.safe ? safe : fast) && ResolutionIndex() == choices.resolution &&
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
    if (choices.frame_gen) {
        Set(v.frame_gen_type, Settings::FrameGenType::Fsr3);
        Set(v.frame_gen_multiplier, 2U);
    }
    if (choices.safe) {
        Set(v.gpu_accuracy, Settings::GpuAccuracy::High);
        Set(v.use_asynchronous_shaders, false);
        Set(v.native_async_draws, Settings::NativeAsyncDraws::Wait);
    }
    restart_needed = true;
    Changed();
    const QStringList names{tr("Best"), tr("Smooth"), tr("Sharp"), tr("Safe")};
    status->setText(tr("%1 mode on. Start the game again for all of it to apply.")
                        .arg(names.value(mode)));
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

} // namespace Fenix
