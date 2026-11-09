// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include "common/common_types.h"

// What Fenix remembers about the library that the emulator's own settings don't: when each game
// was last played and first seen, the user's own order, the chosen sort and filter, and the last
// game for the Continue button. Stored as fenix_library.json in the config folder.
namespace Fenix {

enum class Console {
    Switch,
    Switch2,
};

/// Switch 2 program ids start with 0x04 (Switch 1: 0x01).
Console ConsoleOf(u64 program_id);
QString ConsoleName(Console console);

enum class SortMode : int {
    Recent = 0,
    Name,
    PlayTime,
    Added,
    Custom,
    Count,
};

enum class Filter : int {
    All = 0,
    Favorites,
    Switch2,
    Switch1,
    Count,
};

class Library {
public:
    static Library& Get();

    /// Identifies a game across rescans: its program id, or its path when it has none.
    static QString KeyOf(const QString& path, u64 program_id);

    qint64 LastPlayed(const QString& key) const;
    /// Seconds since epoch the game was first seen (recorded now when unknown).
    qint64 AddedAt(const QString& key);
    void MarkPlayed(const QString& path, u64 program_id);

    QString LastGamePath() const {
        return last_game_path;
    }
    u64 LastGameId() const {
        return last_game_id;
    }

    QStringList CustomOrder() const {
        return custom_order;
    }
    void SetCustomOrder(const QStringList& order);

    SortMode Sort() const {
        return sort;
    }
    void SetSort(SortMode mode);

    Filter CurrentFilter() const {
        return filter;
    }
    void SetFilter(Filter value);

    int CardSize() const {
        return card_size;
    }
    void SetCardSize(int size);

    bool AutoBackup() const {
        return auto_backup;
    }
    void SetAutoBackup(bool enabled);

    int BackupsToKeep() const {
        return backups_to_keep;
    }

    /// Small numbered choices Fenix keeps (performance mode, which first-run defaults ran).
    int Option(const QString& name, int fallback = 0) const {
        return options.value(name, fallback);
    }
    void SetOption(const QString& name, int value);

    /// The user's own name for a game (empty when none).
    QString CustomTitle(const QString& key) const;
    void SetCustomTitle(const QString& key, const QString& title);

    /// The user's own image for a game: a PNG in config/fenix_art (empty path when none).
    QString CustomImagePath(const QString& key) const;
    /// Stores an image (any format Qt reads) as the game's art; false if it can't be read.
    bool SetCustomImage(const QString& key, const QByteArray& image_data);
    void ClearCustomImage(const QString& key);

    void Save();

private:
    Library();
    void Load();

    QString path;
    QHash<QString, qint64> last_played;
    QHash<QString, qint64> added;
    QStringList custom_order;
    QHash<QString, QString> custom_titles;
    QHash<QString, QString> custom_images; ///< key -> file name in art_dir
    QString art_dir;
    QString last_game_path;
    u64 last_game_id{};
    SortMode sort{SortMode::Recent};
    Filter filter{Filter::All};
    QHash<QString, int> options;
    int card_size{200};
    bool auto_backup{true};
    int backups_to_keep{10};
    bool dirty{};
};

} // namespace Fenix
