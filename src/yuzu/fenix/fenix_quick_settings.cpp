// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
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
#include "common/settings.h"
#include "yuzu/configuration/nvidia_dlss_panel.h"
#include "yuzu/fenix/fenix_library.h"
#include "yuzu/fenix/fenix_quick_settings.h"
#include "yuzu/fenix/fenix_theme.h"

namespace Fenix {

namespace {

using Settings::ResolutionSetup;

enum Preset { Performance = 0, Balanced, Quality, Ultra };

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
    Settings::values.resolution_setup.SetValue(values[std::clamp(index, 0, 3)]);
}

} // namespace

QString QuickSettings::RecommendedPreset() {
    using namespace Common::Literals;
    const unsigned threads = std::thread::hardware_concurrency();
    const u64 ram = Common::GetMemInfo().TotalPhysicalMemory;
    if (threads >= 16 && ram >= 30_GiB) {
        return tr("Ultra");
    }
    if (threads >= 12 && ram >= 15_GiB) {
        return tr("Quality");
    }
    if (threads >= 8) {
        return tr("Balanced");
    }
    return tr("Performance");
}

void QuickSettings::ApplyStartupOptions() {
    // Option name, its variable, and the value that variable takes when the option is 1. A
    // variable set before Fenix started (by hand) wins.
    struct Mapping {
        const char* option;
        const char* variable;
        const char* value;
    };
    static const Mapping mappings[] = {
        {"perf_safe", "EDEN_NX2_PERF_SAFE", "1"},
        {"accurate_shaders", "EDEN_NX2_SMOOTH_SHADERS", "0"},
        {"all_ccds", "EDEN_NX2_ONE_CCD", "0"},
        {"no_frame_snapshots", "EDEN_NX2_FRAME_SNAPSHOT", "0"},
    };
    static QSet<QString> set_by_user = [] {
        QSet<QString> result;
        for (const auto& mapping : mappings) {
            if (qEnvironmentVariableIsSet(mapping.variable)) {
                result.insert(QString::fromLatin1(mapping.variable));
            }
        }
        return result;
    }();
    for (const auto& mapping : mappings) {
        if (set_by_user.contains(QString::fromLatin1(mapping.variable))) {
            continue;
        }
        const int chosen = Library::Get().Option(QString::fromLatin1(mapping.option));
        if (chosen == 2 && qstrcmp(mapping.option, "all_ccds") == 0) {
            // CPU cores > Split: GPU threads on one CCD, the game's CPU threads on the other.
            qputenv(mapping.variable, "2");
        } else if (chosen == 1) {
            qputenv(mapping.variable, mapping.value);
        } else {
            qunsetenv(mapping.variable);
        }
    }
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
        tr("Fastest, no flicker. Start here."),
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

    AddRow(layout, tr("Run one frame ahead"),
           tr("Switch 2 games prepare the next frame while the last one still draws: much faster. "
              "On holds back only what would make the screen flicker. Auto: On in Ocarina of "
              "Time. Fastest skips that and can flicker. Changes right away."),
           {tr("Auto"), tr("On"), tr("Fastest"), tr("Off")},
           [] {
               switch (Settings::values.nx2_frame_pacing.GetValue()) {
               case Settings::Nx2FramePacing::OneFrameAhead:
                   return 1;
               case Settings::Nx2FramePacing::OneFrameAheadUnguarded:
                   return 2;
               case Settings::Nx2FramePacing::WaitEachFrame:
                   return 3;
               default:
                   return 0;
               }
           },
           [](int index) {
               static constexpr Settings::Nx2FramePacing values[] = {
                   Settings::Nx2FramePacing::Auto, Settings::Nx2FramePacing::OneFrameAhead,
                   Settings::Nx2FramePacing::OneFrameAheadUnguarded,
                   Settings::Nx2FramePacing::WaitEachFrame};
               Settings::values.nx2_frame_pacing.SetValue(values[std::clamp(index, 0, 3)]);
           });

    const auto option_row = [this, layout](const QString& title, const QString& hint,
                                           const QStringList& options, const char* name) {
        const QString key = QString::fromLatin1(name);
        AddRow(layout, title, hint, options,
               [key, count = static_cast<int>(options.size())] {
                   return std::clamp(Library::Get().Option(key), 0, count - 1);
               },
               [key](int index) {
                   Library::Get().SetOption(key, index);
                   ApplyStartupOptions();
               });
    };
    option_row(tr("Controller"),
               tr("Automatic: a controller you plug in or pair (8BitDo, Xbox, PlayStation, Pro "
                  "Controller) becomes player 1 by itself. Manual: only what you set up in All "
                  "Settings > Controls. Changes right away."),
               {tr("Automatic"), tr("Manual")}, "manual_controller");
    option_row(tr("Shader loading"),
               tr("Smooth: while a new effect's shader compiles, 3D draws skip a few frames "
                  "instead of freezing the game. Accurate: wait for every shader (stutters). "
                  "Applies the next time a game starts."),
               {tr("Smooth"), tr("Accurate")}, "accurate_shaders");
    option_row(tr("CPU cores"),
               tr("Ryzen 9 with two CCDs. One CCD: the emulator's busiest threads share a chiplet "
                  "and talk faster. Split: the graphics threads get one chiplet and the game's "
                  "CPU threads the other, so none share a core. Try both. Applies after "
                  "restarting Fenix."),
               {tr("One CCD"), tr("All"), tr("Split")}, "all_ccds");
    option_row(tr("Switch 2 speed-ups"),
               tr("All: every GPU thread optimization. Safe: without the experimental ones, to try "
                  "when a game glitches or crashes. Applies after restarting Fenix."),
               {tr("All"), tr("Safe")}, "perf_safe");
    option_row(tr("Flicker fix"),
               tr("Copy: Fenix keeps what the last frame still needs while the game writes the "
                  "next one, so nothing waits. Wait: the old way (slower). Applies after "
                  "restarting Fenix."),
               {tr("Copy (fast)"), tr("Wait")}, "no_frame_snapshots");
    option_row(tr("Game tuning"),
               tr("Automatic: games Fenix knows (Ocarina of Time, Mario Kart World) start with "
                  "their best speed settings, no popup. How they look stays yours. Off: only "
                  "your own settings. Applies the next time a game starts."),
               {tr("Automatic"), tr("Off")}, "manual_game_tuning");

    AddRow(layout, tr("Resolution"),
           tr("Switch games: 1x is 1080p docked, 2x is 4K. Keep 1x for Switch 2 games: they "
              "already render sharp, and higher replaces their own upscaler. Applies the next time a "
              "game starts."),
           {tr("1x"), tr("2x (4K)"), tr("3x"), tr("4x")}, ResolutionIndex, SetResolution);

    AddRow(layout, tr("Frame generation"),
           tr("Extra in-between frames for smoother motion (AMD FSR 3). Best when the game "
              "already runs near full speed. Applies the next time a game starts."),
           {tr("Off"), tr("2x"), tr("3x"), tr("4x")},
           [] {
               if (!Settings::values.frame_gen.GetValue()) {
                   return 0;
               }
               return std::clamp<int>(
                   static_cast<int>(Settings::values.frame_gen_multiplier.GetValue()) - 1, 1, 3);
           },
           [](int index) {
               Settings::values.frame_gen.SetValue(index > 0);
               if (index > 0) {
                   Settings::values.frame_gen_type.SetValue(Settings::FrameGenType::Fsr3);
                   Settings::values.frame_gen_multiplier.SetValue(static_cast<u32>(index + 1));
               }
           });

    AddRow(layout, tr("Switch 2 frame rate cap"),
           tr("Game default follows what the game asks for. 60 FPS holds games that drop "
              "themselves to 30 when they run slow. Changes right away."),
           {tr("Game default"), tr("60 FPS"), tr("30 FPS")},
           [] { return static_cast<int>(Settings::values.nx2_swap_interval.GetValue()); },
           [](int index) {
               Settings::values.nx2_swap_interval.SetValue(
                   static_cast<Settings::Nx2SwapInterval>(index));
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
               Settings::values.vsync_mode.SetValue(index == 0 ? Settings::VSyncMode::FifoRelaxed
                                                               : Settings::VSyncMode::Mailbox);
           });

    AddRow(layout, tr("Console mode"),
           tr("Docked renders at TV resolution (higher), handheld at the portable one. Changes right away."),
           {tr("Docked (TV)"), tr("Handheld")},
           [] {
               return Settings::values.use_docked_mode.GetValue() == Settings::ConsoleMode::Docked
                          ? 0
                          : 1;
           },
           [](int index) {
               Settings::values.use_docked_mode.SetValue(index == 0 ? Settings::ConsoleMode::Docked
                                                                    : Settings::ConsoleMode::Handheld);
           });

    AddRow(layout, tr("Anti-aliasing"), tr("Smooths jagged edges. Changes right away."),
           {tr("Off"), tr("FXAA"), tr("SMAA")},
           [] { return static_cast<int>(Settings::values.anti_aliasing.GetValue()); },
           [](int index) {
               Settings::values.anti_aliasing.SetValue(static_cast<Settings::AntiAliasing>(index));
           });

    AddRow(layout, tr("Present on its own thread"),
           tr("Recommended: the emulator's GPU thread doesn't wait for your monitor every frame. "
              "Applies the next time a game starts."),
           {tr("On"), tr("Off")},
           [] { return Settings::values.async_presentation.GetValue() ? 0 : 1; },
           [](int index) { Settings::values.async_presentation.SetValue(index == 0); });

    AddRow(layout, tr("NVIDIA DLSS"),
           tr("RTX cards, Switch 2 games: real DLSS in place of the game's own anti-aliasing. "
              "One-time setup: the download button right below. Applies the next time a game "
              "starts."),
           {tr("Off"), tr("On")},
           [] { return Settings::values.nvidia_dlss.GetValue() ? 1 : 0; },
           [](int index) { Settings::values.nvidia_dlss.SetValue(index == 1); });

#ifdef EGLANTINE_STREAMLINE
    // Whether NVIDIA's DLSS files are there, with the button that downloads them.
    layout->addWidget(new NvidiaDlssPanel(body));
#endif

    AddRow(layout, tr("DLSS quality"),
           tr("DLAA is the sharpest: full resolution with DLSS anti-aliasing. Applies the next "
              "time a game starts."),
           {tr("Performance"), tr("Balanced"), tr("Quality"), tr("DLAA")},
           [] { return static_cast<int>(Settings::values.nvidia_dlss_quality.GetValue()); },
           [](int index) {
               Settings::values.nvidia_dlss_quality.SetValue(
                   static_cast<Settings::DlssQuality>(index));
           });


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
    connect(group, &QButtonGroup::idClicked, this, [this, apply](int id) {
        apply(id);
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

void QuickSettings::ApplyPreset(int preset) {
    auto& v = Settings::values;
    // Shared by every preset: the settings that only help.
    v.gpu_accuracy.SetValue(Settings::GpuAccuracy::Low);
    v.use_asynchronous_shaders.SetValue(true);
    v.use_disk_shader_cache.SetValue(true);
    v.async_presentation.SetValue(true);
    v.astc_recompression.SetValue(Settings::AstcRecompression::Bc3);
    switch (preset) {
    case Performance:
        SetResolution(0);
        v.anti_aliasing.SetValue(Settings::AntiAliasing::None);
        v.scaling_filter.SetValue(Settings::ScalingFilter::Bilinear);
        v.max_anisotropy.SetValue(Settings::AnisotropyMode::Default);
        break;
    case Balanced:
        SetResolution(0);
        v.anti_aliasing.SetValue(Settings::AntiAliasing::Fxaa);
        v.scaling_filter.SetValue(Settings::ScalingFilter::Fsr);
        v.max_anisotropy.SetValue(Settings::AnisotropyMode::X4);
        break;
    case Quality:
        SetResolution(1);
        v.anti_aliasing.SetValue(Settings::AntiAliasing::Smaa);
        v.scaling_filter.SetValue(Settings::ScalingFilter::Fsr);
        v.max_anisotropy.SetValue(Settings::AnisotropyMode::X16);
        break;
    case Ultra:
    default:
        SetResolution(2);
        v.anti_aliasing.SetValue(Settings::AntiAliasing::Smaa);
        v.scaling_filter.SetValue(Settings::ScalingFilter::Fsr);
        v.max_anisotropy.SetValue(Settings::AnisotropyMode::X16);
        break;
    }
    Changed();
    const QStringList names{tr("Performance"), tr("Balanced"), tr("Quality"), tr("Ultra")};
    status->setText(tr("%1 preset applied.").arg(names.value(preset)));
}

namespace {
/// The Quick Settings choices in the library file that modes set (0 is their best). CPU cores
/// and the controller choice depend on the PC, not the mode: modes leave them alone.
constexpr const char* best_options[] = {"accurate_shaders", "perf_safe", "no_frame_snapshots",
                                        "manual_game_tuning"};
/// Of those, the ones read when Fenix starts.
constexpr const char* restart_options[] = {"perf_safe", "no_frame_snapshots"};
} // namespace

namespace {
enum Mode { Best, Smooth, Sharp, Safe, ModeCount };

/// What a mode sets beyond the choices every mode shares.
struct ModeChoices {
    int resolution;                      ///< Index for SetResolution
    Settings::AntiAliasing anti_aliasing;
    Settings::AnisotropyMode anisotropy;
    bool frame_gen;
    Settings::Nx2FramePacing pacing;
    bool safe; ///< Safe speed-ups, accurate shaders, the waiting flicker fix
};

ModeChoices ChoicesOf(int mode) {
    switch (mode) {
    case Smooth:
        return {0, Settings::AntiAliasing::Fxaa, Settings::AnisotropyMode::X4, true,
                Settings::Nx2FramePacing::Auto, false};
    case Sharp:
        return {1, Settings::AntiAliasing::Smaa, Settings::AnisotropyMode::X16, false,
                Settings::Nx2FramePacing::Auto, false};
    case Safe:
        return {0, Settings::AntiAliasing::Fxaa, Settings::AnisotropyMode::X4, false,
                Settings::Nx2FramePacing::WaitEachFrame, true};
    default:
        return {0, Settings::AntiAliasing::Fxaa, Settings::AnisotropyMode::X4, false,
                Settings::Nx2FramePacing::Auto, false};
    }
}

int OptionValue(const char* option, bool safe) {
    const QString name = QString::fromLatin1(option);
    const bool safe_option = name == QStringLiteral("perf_safe") ||
                             name == QStringLiteral("accurate_shaders") ||
                             name == QStringLiteral("no_frame_snapshots");
    return safe && safe_option ? 1 : 0;
}
} // namespace

int QuickSettings::CurrentMode() {
    const auto& v = Settings::values;
    const auto vsync = v.vsync_mode.GetValue();
    const bool shared =
        static_cast<int>(v.nx2_swap_interval.GetValue()) == 0 && v.async_presentation.GetValue() &&
        v.use_asynchronous_shaders.GetValue() && v.use_disk_shader_cache.GetValue() &&
        v.gpu_accuracy.GetValue() == Settings::GpuAccuracy::Low &&
        v.astc_recompression.GetValue() == Settings::AstcRecompression::Bc3 &&
        (vsync == Settings::VSyncMode::Fifo || vsync == Settings::VSyncMode::FifoRelaxed) &&
        v.use_docked_mode.GetValue() == Settings::ConsoleMode::Docked;
    if (!shared) {
        return -1;
    }
    for (int mode = 0; mode < ModeCount; ++mode) {
        const ModeChoices choices = ChoicesOf(mode);
        bool match = ResolutionIndex() == choices.resolution &&
                     v.anti_aliasing.GetValue() == choices.anti_aliasing &&
                     v.frame_gen.GetValue() == choices.frame_gen &&
                     v.nx2_frame_pacing.GetValue() == choices.pacing;
        for (const char* option : best_options) {
            match = match && Library::Get().Option(QString::fromLatin1(option)) ==
                                 OptionValue(option, choices.safe);
        }
        if (match) {
            return mode;
        }
    }
    return -1;
}

void QuickSettings::ApplyMode(int mode) {
    const ModeChoices choices = ChoicesOf(mode);
    bool needs_restart = false;
    for (const char* option : restart_options) {
        needs_restart |= Library::Get().Option(QString::fromLatin1(option)) !=
                         OptionValue(option, choices.safe);
    }
    for (const char* option : best_options) {
        Library::Get().SetOption(QString::fromLatin1(option), OptionValue(option, choices.safe));
    }
    ApplyStartupOptions();
    auto& v = Settings::values;
    v.nx2_frame_pacing.SetValue(choices.pacing);
    v.nx2_swap_interval.SetValue(static_cast<Settings::Nx2SwapInterval>(0));
    v.async_presentation.SetValue(true);
    v.use_asynchronous_shaders.SetValue(true);
    v.use_disk_shader_cache.SetValue(true);
    v.gpu_accuracy.SetValue(Settings::GpuAccuracy::Low);
    v.astc_recompression.SetValue(Settings::AstcRecompression::Bc3);
    v.vsync_mode.SetValue(Settings::VSyncMode::FifoRelaxed);
    v.use_docked_mode.SetValue(Settings::ConsoleMode::Docked);
    SetResolution(choices.resolution);
    v.anti_aliasing.SetValue(choices.anti_aliasing);
    v.scaling_filter.SetValue(Settings::ScalingFilter::Fsr);
    v.max_anisotropy.SetValue(choices.anisotropy);
    v.frame_gen.SetValue(choices.frame_gen);
    if (choices.frame_gen) {
        v.frame_gen_type.SetValue(Settings::FrameGenType::Fsr3);
        v.frame_gen_multiplier.SetValue(2U);
    }
    Changed();
    const QStringList names{tr("Best"), tr("Smooth"), tr("Sharp"), tr("Safe")};
    status->setText(needs_restart
                        ? tr("%1 mode on. Restart Fenix to finish.").arg(names.value(mode))
                        : tr("%1 mode on. Start the game again for all of it to apply.")
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
    resize(width(), show ? 860 : 400);
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
