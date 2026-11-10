// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Menu sounds: short tones Fenix makes itself (no recorded or borrowed sounds), played without
// waiting. On by default; Quick Settings > Advanced turns them off.
namespace Fenix::Feedback {

enum class Sound {
    Move,     ///< The selection moved
    Select,   ///< Something was chosen or opened
    Back,     ///< Went back or closed
    Favorite, ///< A game was made a favorite
    Launch,   ///< A game starts
};

void Play(Sound sound);

bool SoundsEnabled();
bool RumbleEnabled();

} // namespace Fenix::Feedback
