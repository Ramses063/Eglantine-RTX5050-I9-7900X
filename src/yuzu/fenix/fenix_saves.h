// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Save data backups: before a game starts, Fenix copies its save folders (every folder named after
// its program id under the save directory: account saves of each user, device saves) to
// <backup root>/<program id>/<date-time>/, keeping the newest few. A backup can be put back from
// the game's menu. Plain std::filesystem, no emulator state: it runs while no game does.
namespace Fenix::Saves {

struct Backup {
    std::filesystem::path directory; ///< <backup root>/<program id>/<label>
    std::string label;               ///< "2026-11-09 14:52:07"
    std::uintmax_t bytes{};
};

/// The save folders of a title, relative to save_root.
std::vector<std::filesystem::path> FindSaveDirs(const std::filesystem::path& save_root,
                                                std::uint64_t program_id);

/// Copies the title's saves into a new backup and deletes all but the newest `keep`. Returns the
/// backup's folder, or nothing when the title has no saves yet (or on error, with `error` set).
std::optional<std::filesystem::path> BackupNow(const std::filesystem::path& save_root,
                                               const std::filesystem::path& backup_root,
                                               std::uint64_t program_id, int keep,
                                               std::string* error = nullptr);

/// The title's backups, newest first.
std::vector<Backup> List(const std::filesystem::path& backup_root, std::uint64_t program_id);

/// Puts a backup's folders back over the current saves (each folder replaced as a whole).
bool Restore(const std::filesystem::path& save_root, const Backup& backup,
             std::string* error = nullptr);

} // namespace Fenix::Saves
