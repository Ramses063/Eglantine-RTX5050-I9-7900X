// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>

namespace VideoCommon {

/// True while the buffer cache keeps guest pages for a frame still on the GPU threads (frame
/// snapshots, buffer_cache_base.h). Writes the GPU threads make to guest memory check it to keep
/// those pages current.
inline std::atomic<bool> g_nx2_frame_snapshots_live{false};

/// Set on a GPU thread while it reads guest memory for a texture upload: those reads take the
/// kept bytes of pages the game has already rewritten for the next frame.
inline thread_local bool t_nx2_overlay_reads = false;

struct Nx2OverlayReadsScope {
    Nx2OverlayReadsScope() : previous{t_nx2_overlay_reads} {
        t_nx2_overlay_reads = true;
    }
    ~Nx2OverlayReadsScope() {
        t_nx2_overlay_reads = previous;
    }
    Nx2OverlayReadsScope(const Nx2OverlayReadsScope&) = delete;
    Nx2OverlayReadsScope& operator=(const Nx2OverlayReadsScope&) = delete;
    bool previous;
};

[[nodiscard]] inline bool Nx2OverlayingReads() {
    return t_nx2_overlay_reads && g_nx2_frame_snapshots_live.load(std::memory_order_relaxed);
}

} // namespace VideoCommon
