// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <system_error>

#include "yuzu/fenix/fenix_saves.h"

namespace fs = std::filesystem;

namespace Fenix::Saves {

namespace {

// Backups this large are skipped: a save folder is a few MB; anything bigger is not a save.
constexpr std::uintmax_t MaxBackupBytes = 2ULL << 30;
constexpr int MaxDepth = 8;

std::string HexId(std::uint64_t program_id) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llX", static_cast<unsigned long long>(program_id));
    return text;
}

bool SameIgnoringCase(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::toupper(static_cast<unsigned char>(x)) ==
                      std::toupper(static_cast<unsigned char>(y));
           });
}

std::uintmax_t FolderSize(const fs::path& folder) {
    std::error_code ec;
    std::uintmax_t total = 0;
    for (fs::recursive_directory_iterator it{folder, fs::directory_options::skip_permission_denied,
                                             ec},
         end;
         !ec && it != end; it.increment(ec)) {
        std::error_code size_ec;
        if (it->is_regular_file(size_ec)) {
            total += it->file_size(size_ec);
        }
    }
    return total;
}

std::string NowLabel() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d_%H-%M-%S", &local);
    return text;
}

void SetError(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

std::vector<fs::path> FindSaveDirs(const fs::path& save_root, std::uint64_t program_id) {
    std::vector<fs::path> found;
    const std::string id = HexId(program_id);
    std::error_code ec;
    fs::recursive_directory_iterator it{save_root, fs::directory_options::skip_permission_denied,
                                        ec};
    for (const fs::recursive_directory_iterator end; !ec && it != end; it.increment(ec)) {
        std::error_code dir_ec;
        if (!it->is_directory(dir_ec)) {
            continue;
        }
        if (SameIgnoringCase(it->path().filename().string(), id)) {
            found.push_back(fs::relative(it->path(), save_root, dir_ec));
            it.disable_recursion_pending(); // a save's own subfolders are not other saves
            continue;
        }
        if (it.depth() >= MaxDepth) {
            it.disable_recursion_pending();
        }
    }
    return found;
}

std::optional<fs::path> BackupNow(const fs::path& save_root, const fs::path& backup_root,
                                  std::uint64_t program_id, int keep, std::string* error) {
    const auto saves = FindSaveDirs(save_root, program_id);
    if (saves.empty()) {
        return std::nullopt;
    }
    std::uintmax_t total = 0;
    for (const auto& relative : saves) {
        total += FolderSize(save_root / relative);
    }
    if (total == 0) {
        return std::nullopt;
    }
    if (total > MaxBackupBytes) {
        SetError(error, "save folders are larger than 2 GiB, not backed up");
        return std::nullopt;
    }
    const fs::path title_root = backup_root / HexId(program_id);
    fs::path target = title_root / NowLabel();
    std::error_code ec;
    for (int suffix = 2; fs::exists(target, ec); ++suffix) {
        target = title_root / (NowLabel() + "_" + std::to_string(suffix));
    }
    for (const auto& relative : saves) {
        const fs::path destination = target / relative;
        fs::create_directories(destination, ec);
        if (ec) {
            SetError(error, "cannot create " + destination.string() + ": " + ec.message());
            return std::nullopt;
        }
        fs::copy(save_root / relative, destination,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec) {
            SetError(error, "copy failed: " + ec.message());
            fs::remove_all(target, ec);
            return std::nullopt;
        }
    }
    // Keep the newest `keep` backups (labels sort by time).
    auto backups = List(backup_root, program_id);
    for (size_t i = static_cast<size_t>(std::max(keep, 1)); i < backups.size(); ++i) {
        fs::remove_all(backups[i].directory, ec);
    }
    return target;
}

std::vector<Backup> List(const fs::path& backup_root, std::uint64_t program_id) {
    std::vector<Backup> backups;
    const fs::path title_root = backup_root / HexId(program_id);
    std::error_code ec;
    for (fs::directory_iterator it{title_root, ec}, end; !ec && it != end; it.increment(ec)) {
        std::error_code dir_ec;
        if (!it->is_directory(dir_ec)) {
            continue;
        }
        Backup backup;
        backup.directory = it->path();
        std::string name = it->path().filename().string();
        // "2026-11-09_14-52-07" -> "2026-11-09 14:52:07"
        if (name.size() >= 19 && name[10] == '_') {
            name[10] = ' ';
            name[13] = ':';
            name[16] = ':';
        }
        backup.label = name;
        backup.bytes = FolderSize(it->path());
        backups.push_back(std::move(backup));
    }
    std::sort(backups.begin(), backups.end(), [](const Backup& a, const Backup& b) {
        return a.directory.filename() > b.directory.filename();
    });
    return backups;
}

bool Restore(const fs::path& save_root, const Backup& backup, std::string* error) {
    std::error_code ec;
    // The backup holds the save folders at their paths relative to the save root: find them the
    // same way (folders named after the program id).
    const std::string id = backup.directory.parent_path().filename().string();
    std::vector<fs::path> folders;
    fs::recursive_directory_iterator it{backup.directory,
                                        fs::directory_options::skip_permission_denied, ec};
    for (const fs::recursive_directory_iterator end; !ec && it != end; it.increment(ec)) {
        std::error_code dir_ec;
        if (it->is_directory(dir_ec) && SameIgnoringCase(it->path().filename().string(), id)) {
            folders.push_back(fs::relative(it->path(), backup.directory, dir_ec));
            it.disable_recursion_pending();
        }
    }
    if (folders.empty()) {
        SetError(error, "the backup holds no save folders");
        return false;
    }
    for (const auto& relative : folders) {
        const fs::path destination = save_root / relative;
        fs::remove_all(destination, ec);
        fs::create_directories(destination, ec);
        fs::copy(backup.directory / relative, destination,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec) {
            SetError(error, "restore failed at " + destination.string() + ": " + ec.message());
            return false;
        }
    }
    return true;
}

} // namespace Fenix::Saves
