// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "yuzu/fenix/fenix_feedback.h"
#include "yuzu/fenix/fenix_library.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace Fenix::Feedback {

namespace {

constexpr int SampleRate = 44100;
constexpr double Pi = 3.14159265358979323846;

struct Note {
    double frequency;
    double start; ///< seconds
    double length; ///< seconds
    double gain;
};

/// A mono 16-bit WAV file of soft sine notes (a touch of the octave for warmth, quick attack,
/// smooth decay).
std::vector<std::uint8_t> Render(const std::vector<Note>& notes, double total) {
    const int samples = static_cast<int>(total * SampleRate);
    std::vector<double> mix(static_cast<size_t>(samples), 0.0);
    for (const Note& note : notes) {
        const int begin = static_cast<int>(note.start * SampleRate);
        const int count = static_cast<int>(note.length * SampleRate);
        for (int i = 0; i < count && begin + i < samples; ++i) {
            const double t = static_cast<double>(i) / SampleRate;
            const double attack = std::min(1.0, t / 0.004);
            const double decay = std::exp(-t * 5.5 / note.length);
            const double tail = std::min(1.0, static_cast<double>(count - i) / (0.006 * SampleRate));
            const double wave = std::sin(2 * Pi * note.frequency * t) +
                                0.18 * std::sin(4 * Pi * note.frequency * t);
            mix[static_cast<size_t>(begin + i)] += note.gain * attack * decay * tail * wave;
        }
    }
    std::vector<std::uint8_t> wav(44 + static_cast<size_t>(samples) * 2);
    const auto put32 = [&wav](size_t at, std::uint32_t value) { std::memcpy(&wav[at], &value, 4); };
    const auto put16 = [&wav](size_t at, std::uint16_t value) { std::memcpy(&wav[at], &value, 2); };
    std::memcpy(&wav[0], "RIFF", 4);
    put32(4, static_cast<std::uint32_t>(wav.size() - 8));
    std::memcpy(&wav[8], "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1); // PCM
    put16(22, 1); // mono
    put32(24, SampleRate);
    put32(28, SampleRate * 2);
    put16(32, 2);
    put16(34, 16);
    std::memcpy(&wav[36], "data", 4);
    put32(40, static_cast<std::uint32_t>(samples * 2));
    for (int i = 0; i < samples; ++i) {
        const double value = std::clamp(mix[static_cast<size_t>(i)], -1.0, 1.0);
        put16(44 + static_cast<size_t>(i) * 2,
              static_cast<std::uint16_t>(static_cast<std::int16_t>(value * 32767)));
    }
    return wav;
}

const std::vector<std::uint8_t>& Wave(Sound sound) {
    static const std::array<std::vector<std::uint8_t>, 5> waves{
        // Move: one short, soft tick.
        Render({{1760, 0, 0.035, 0.12}}, 0.045),
        // Select: two notes going up.
        Render({{880, 0, 0.07, 0.16}, {1318.5, 0.055, 0.11, 0.16}}, 0.17),
        // Back: two notes going down.
        Render({{1174.7, 0, 0.07, 0.14}, {784, 0.055, 0.1, 0.14}}, 0.16),
        // Favorite: a quick bright sparkle.
        Render({{1318.5, 0, 0.06, 0.12}, {1568, 0.045, 0.06, 0.12}, {2093, 0.09, 0.14, 0.12}},
               0.24),
        // Launch: a rising chord.
        Render({{523.3, 0, 0.35, 0.12},
                {659.3, 0.05, 0.32, 0.11},
                {784, 0.1, 0.3, 0.1},
                {1046.5, 0.15, 0.32, 0.09}},
               0.48),
    };
    return waves[static_cast<size_t>(sound)];
}

} // namespace

bool SoundsEnabled() {
    return Library::Get().Option(QStringLiteral("ui_sounds"), 1) != 0;
}

bool RumbleEnabled() {
    return Library::Get().Option(QStringLiteral("ui_rumble"), 1) != 0;
}

void Play(Sound sound) {
    if (!SoundsEnabled()) {
        return;
    }
#ifdef _WIN32
    // Async from memory: returns at once; a new sound replaces one still playing.
    PlaySoundW(reinterpret_cast<LPCWSTR>(Wave(sound).data()), nullptr,
               SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
    (void)Wave(sound);
#endif
}

} // namespace Fenix::Feedback
