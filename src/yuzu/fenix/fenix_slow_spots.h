// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include <QString>

#include "common/common_types.h"

class QWidget;

// Slow spots: while a game runs, Fenix watches its frame rate. When it stays well under what the
// game usually reaches for a few seconds, Fenix keeps a picture of that moment with the numbers, so
// the places where a game slows down can be found and compared later (the game's menu > Slow
// Spots, and the debug report).
namespace Fenix::SlowSpots {

struct Spot {
    qint64 when{};   ///< Seconds since the epoch
    double fps{};    ///< The lowest frame rate of the slow stretch
    double target{}; ///< What the game reached before it
    QString image;   ///< Full path of the picture (may not exist yet, or failed)
};

/// The game's slow spots, newest first.
std::vector<Spot> List(u64 program_id);

/// Lines for the debug report (newest first, at most `count`).
std::string ReportLines(u64 program_id, size_t count = 10);

/// Shows the game's slow spots with their pictures.
void ShowDialog(QWidget* parent, u64 program_id, const QString& title);

/// Watches the frame rate of the running game.
class Tracker {
public:
    void Start(u64 program_id);
    void Stop();

    /// One frame rate reading (twice a second). `target` is the frame rate the game should run at
    /// when known (0: learnt from the game). Returns where to save a picture of this moment when
    /// a slow spot was just recorded, otherwise an empty string.
    QString Sample(double fps, double target);

private:
    u64 program_id{};
    int samples{};
    std::vector<double> history; ///< Recent readings, for the usual frame rate
    int slow_streak{};
    double streak_min{};
    qint64 last_spot{};
    int spots{};
};

} // namespace Fenix::SlowSpots
