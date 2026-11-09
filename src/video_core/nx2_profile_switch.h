// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdlib>

#include "common/common_types.h"

// The on/off switch of the GPU thread zone timers (nx2_profile.h), on its own so the frontend
// can flip it without the rest of that header.
namespace VideoCommon::Nx2Profile {

// EDEN_NX2_GPU_PROFILE at startup, or the "Collect performance diagnostics" setting when a game
// starts (SetEnabled). Enabled() runs several times per method write: a relaxed load is a plain
// read on x86-64 and ARM64.
inline std::atomic<bool> g_enabled{std::getenv("EDEN_NX2_GPU_PROFILE") != nullptr};
inline const bool g_env_enabled = std::getenv("EDEN_NX2_GPU_PROFILE") != nullptr;

inline bool Enabled() {
    return g_enabled.load(std::memory_order_relaxed);
}

/// Counts the times the timers were turned on: a thread's window from before a pause is dropped.
inline std::atomic<u32> g_epoch{1};

/// Turns the zone timers on or off (the variable keeps them on).
inline void SetEnabled(bool enabled) {
    const bool on = enabled || g_env_enabled;
    if (on && !g_enabled.load(std::memory_order_relaxed)) {
        g_epoch.fetch_add(1, std::memory_order_relaxed);
    }
    g_enabled.store(on, std::memory_order_relaxed);
}

} // namespace VideoCommon::Nx2Profile
