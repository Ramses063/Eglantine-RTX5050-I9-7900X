// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include "common/fs/fs_util.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "yuzu/fenix/fenix_library.h"

namespace Fenix {

Console ConsoleOf(u64 program_id) {
    return (program_id >> 56) == 0x04 ? Console::Switch2 : Console::Switch;
}

QString ConsoleName(Console console) {
    return console == Console::Switch2 ? QStringLiteral("Switch 2") : QStringLiteral("Switch");
}

Library& Library::Get() {
    static Library library;
    return library;
}

Library::Library() {
    const auto config_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir);
    path = QString::fromStdString(Common::FS::PathToUTF8String(config_dir / "fenix_library.json"));
    art_dir = QString::fromStdString(Common::FS::PathToUTF8String(config_dir / "fenix_art"));
    Load();
}

QString Library::KeyOf(const QString& game_path, u64 program_id) {
    if (program_id != 0) {
        return QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
    }
    return game_path;
}

void Library::Load() {
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        LOG_WARNING(Frontend, "Fenix: {} is not a JSON object, starting fresh", path.toStdString());
        return;
    }
    const auto root = document.object();
    const auto read_times = [&root](const char* name, QHash<QString, qint64>& out) {
        const auto object = root.value(QLatin1String(name)).toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            out.insert(it.key(), static_cast<qint64>(it.value().toDouble()));
        }
    };
    read_times("last_played", last_played);
    read_times("added", added);
    for (const auto& value : root.value(QStringLiteral("custom_order")).toArray()) {
        custom_order.append(value.toString());
    }
    const auto read_strings = [&root](const char* name, QHash<QString, QString>& out) {
        const auto object = root.value(QLatin1String(name)).toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            out.insert(it.key(), it.value().toString());
        }
    };
    read_strings("custom_titles", custom_titles);
    read_strings("custom_images", custom_images);
    last_game_path = root.value(QStringLiteral("last_game_path")).toString();
    last_game_id = root.value(QStringLiteral("last_game_id")).toString().toULongLong(nullptr, 16);
    sort = static_cast<SortMode>(std::clamp(root.value(QStringLiteral("sort")).toInt(0), 0,
                                            static_cast<int>(SortMode::Count) - 1));
    filter = static_cast<Filter>(std::clamp(root.value(QStringLiteral("filter")).toInt(0), 0,
                                            static_cast<int>(Filter::Count) - 1));
    card_size = std::clamp(root.value(QStringLiteral("card_size")).toInt(176), 120, 320);
    auto_backup = root.value(QStringLiteral("auto_backup")).toBool(true);
    backups_to_keep = std::clamp(root.value(QStringLiteral("backups_to_keep")).toInt(10), 1, 100);
}

void Library::Save() {
    if (!dirty) {
        return;
    }
    QJsonObject root;
    const auto write_times = [&root](const char* name, const QHash<QString, qint64>& in) {
        QJsonObject object;
        for (auto it = in.begin(); it != in.end(); ++it) {
            object.insert(it.key(), static_cast<double>(it.value()));
        }
        root.insert(QLatin1String(name), object);
    };
    write_times("last_played", last_played);
    write_times("added", added);
    root.insert(QStringLiteral("custom_order"), QJsonArray::fromStringList(custom_order));
    const auto write_strings = [&root](const char* name, const QHash<QString, QString>& in) {
        QJsonObject object;
        for (auto it = in.begin(); it != in.end(); ++it) {
            object.insert(it.key(), it.value());
        }
        root.insert(QLatin1String(name), object);
    };
    write_strings("custom_titles", custom_titles);
    write_strings("custom_images", custom_images);
    root.insert(QStringLiteral("last_game_path"), last_game_path);
    root.insert(QStringLiteral("last_game_id"),
                QStringLiteral("%1").arg(last_game_id, 16, 16, QLatin1Char('0')));
    root.insert(QStringLiteral("sort"), static_cast<int>(sort));
    root.insert(QStringLiteral("filter"), static_cast<int>(filter));
    root.insert(QStringLiteral("card_size"), card_size);
    root.insert(QStringLiteral("auto_backup"), auto_backup);
    root.insert(QStringLiteral("backups_to_keep"), backups_to_keep);

    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_WARNING(Frontend, "Fenix: cannot write {}", path.toStdString());
        return;
    }
    file.write(QJsonDocument{root}.toJson(QJsonDocument::Indented));
    if (file.commit()) {
        dirty = false;
    }
}

qint64 Library::LastPlayed(const QString& key) const {
    return last_played.value(key, 0);
}

qint64 Library::AddedAt(const QString& key) {
    const auto found = added.constFind(key);
    if (found != added.constEnd()) {
        return found.value();
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    added.insert(key, now);
    dirty = true;
    return now;
}

void Library::MarkPlayed(const QString& game_path, u64 program_id) {
    last_played.insert(KeyOf(game_path, program_id), QDateTime::currentSecsSinceEpoch());
    last_game_path = game_path;
    last_game_id = program_id;
    dirty = true;
    Save();
}

void Library::SetCustomOrder(const QStringList& order) {
    custom_order = order;
    dirty = true;
    Save();
}

void Library::SetSort(SortMode mode) {
    sort = mode;
    dirty = true;
    Save();
}

void Library::SetFilter(Filter value) {
    filter = value;
    dirty = true;
    Save();
}

void Library::SetCardSize(int size) {
    card_size = std::clamp(size, 120, 320);
    dirty = true;
    Save();
}

void Library::SetAutoBackup(bool enabled) {
    auto_backup = enabled;
    dirty = true;
    Save();
}

QString Library::CustomTitle(const QString& key) const {
    return custom_titles.value(key);
}

void Library::SetCustomTitle(const QString& key, const QString& title) {
    if (title.trimmed().isEmpty()) {
        custom_titles.remove(key);
    } else {
        custom_titles.insert(key, title.trimmed());
    }
    dirty = true;
    Save();
}

QString Library::CustomImagePath(const QString& key) const {
    const QString file = custom_images.value(key);
    return file.isEmpty() ? QString{} : art_dir + QLatin1Char('/') + file;
}

bool Library::SetCustomImage(const QString& key, const QByteArray& image_data) {
    QImage image;
    if (!image.loadFromData(image_data) || image.isNull()) {
        return false;
    }
    // Square, centred, at most 512 pixels: the cards show square art.
    const int side = std::min(image.width(), image.height());
    image = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side);
    if (side > 512) {
        image = image.scaled(512, 512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    QDir{}.mkpath(art_dir);
    // A new file name each time, so cached pixmaps of the old image are never shown again.
    QString safe_key = key;
    safe_key.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    const QString file =
        QStringLiteral("%1-%2.png").arg(safe_key.right(60)).arg(QDateTime::currentMSecsSinceEpoch());
    if (!image.save(art_dir + QLatin1Char('/') + file, "PNG")) {
        return false;
    }
    ClearCustomImage(key);
    custom_images.insert(key, file);
    dirty = true;
    Save();
    return true;
}

void Library::ClearCustomImage(const QString& key) {
    const QString old_path = CustomImagePath(key);
    if (!old_path.isEmpty()) {
        QFile::remove(old_path);
    }
    if (custom_images.remove(key) > 0) {
        dirty = true;
        Save();
    }
}

} // namespace Fenix
