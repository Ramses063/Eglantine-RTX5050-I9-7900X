// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

// Lines collected while a game runs for Help > Copy Debug Report: each section keeps its last
// few lines (frame timing windows, GPU thread profile windows, mode changes). Header-only and
// cheap: writers add a line every few seconds, never per frame.
namespace Common::DebugReport {

struct Store {
    std::mutex mutex;
    std::map<std::string, std::deque<std::string>, std::less<>> sections;
};

inline Store& Instance() {
    static Store store;
    return store;
}

/// Appends a line to a section, keeping its last `keep` lines.
inline void Add(std::string_view section, std::string line, std::size_t keep = 12) {
    auto& store = Instance();
    std::scoped_lock lock{store.mutex};
    auto it = store.sections.find(section);
    if (it == store.sections.end()) {
        it = store.sections.emplace(std::string{section}, std::deque<std::string>{}).first;
    }
    it->second.push_back(std::move(line));
    while (it->second.size() > keep) {
        it->second.pop_front();
    }
}

/// Replaces a section with one line.
inline void Set(std::string_view section, std::string line) {
    Add(section, std::move(line), 1);
}

/// Empties the game's sections when a new game starts; sections named "Host..." (the machine,
/// the renderer, set before the game's GPU thread starts) stay.
inline void Clear() {
    auto& store = Instance();
    std::scoped_lock lock{store.mutex};
    std::erase_if(store.sections, [](const auto& section) {
        return !section.first.starts_with("Host");
    });
}

/// Every section as text, oldest line first.
inline std::string Dump() {
    auto& store = Instance();
    std::scoped_lock lock{store.mutex};
    std::string out;
    for (const auto& [name, lines] : store.sections) {
        out += "## ";
        out += name;
        out += '\n';
        for (const auto& line : lines) {
            out += line;
            out += '\n';
        }
        out += '\n';
    }
    return out;
}

} // namespace Common::DebugReport
