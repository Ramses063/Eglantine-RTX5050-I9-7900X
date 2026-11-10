// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>

namespace VideoCommon {

/// True while the buffer cache keeps guest pages for a frame still on the GPU threads (frame
/// snapshots, buffer_cache_base.h). Writes the GPU threads make to guest memory check it to keep
/// those pages current.
inline std::atomic<bool> g_nx2_frame_snapshots_live{false};

} // namespace VideoCommon
