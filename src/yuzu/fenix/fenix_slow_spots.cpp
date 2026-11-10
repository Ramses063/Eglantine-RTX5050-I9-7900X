// SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <filesystem>

#include <fmt/format.h>

#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "common/fs/fs_util.h"
#include "common/fs/path_util.h"
#include "yuzu/fenix/fenix_slow_spots.h"
#include "yuzu/fenix/fenix_theme.h"

namespace Fenix::SlowSpots {

namespace {

constexpr int StreakSamples = 6;      ///< 3 seconds of readings under the line
constexpr double SlowRatio = 0.8;     ///< Under 80% of the usual frame rate
constexpr int WarmupSamples = 60;     ///< The first 30 seconds (loading, shaders) don't count
constexpr qint64 SpotGapSeconds = 30; ///< At most one spot every 30 seconds
constexpr int MaxSpotsPerSession = 20;
constexpr int MaxKept = 40;
constexpr size_t HistorySize = 240; ///< Two minutes of readings

QString Folder(u64 program_id) {
    const auto root = Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir) / "fenix_slow_spots" /
                      fmt::format("{:016X}", program_id);
    return QString::fromStdString(Common::FS::PathToUTF8String(root));
}

QString IndexPath(u64 program_id) {
    return Folder(program_id) + QStringLiteral("/spots.json");
}

void Save(u64 program_id, const std::vector<Spot>& spots) {
    QJsonArray array;
    for (const Spot& spot : spots) {
        QJsonObject object;
        object.insert(QStringLiteral("when"), static_cast<double>(spot.when));
        object.insert(QStringLiteral("fps"), spot.fps);
        object.insert(QStringLiteral("target"), spot.target);
        object.insert(QStringLiteral("image"), QFileInfo(spot.image).fileName());
        array.append(object);
    }
    QDir{}.mkpath(Folder(program_id));
    QFile file{IndexPath(program_id)};
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    }
}

} // namespace

std::vector<Spot> List(u64 program_id) {
    std::vector<Spot> spots;
    QFile file{IndexPath(program_id)};
    if (!file.open(QIODevice::ReadOnly)) {
        return spots;
    }
    const auto array = QJsonDocument::fromJson(file.readAll()).array();
    for (const auto& value : array) {
        const QJsonObject object = value.toObject();
        Spot spot;
        spot.when = static_cast<qint64>(object.value(QStringLiteral("when")).toDouble());
        spot.fps = object.value(QStringLiteral("fps")).toDouble();
        spot.target = object.value(QStringLiteral("target")).toDouble();
        spot.image = Folder(program_id) + QLatin1Char('/') +
                     object.value(QStringLiteral("image")).toString();
        spots.push_back(spot);
    }
    std::ranges::sort(spots, [](const Spot& a, const Spot& b) { return a.when > b.when; });
    return spots;
}

std::string ReportLines(u64 program_id, size_t count) {
    std::string lines;
    const auto spots = List(program_id);
    for (size_t i = 0; i < spots.size() && i < count; ++i) {
        const Spot& spot = spots[i];
        lines += fmt::format(
            "{} | {:.0f} of {:.0f} FPS | {}\n",
            QDateTime::fromSecsSinceEpoch(spot.when).toString(Qt::ISODate).toStdString(),
            spot.fps, spot.target, QFileInfo(spot.image).fileName().toStdString());
    }
    return lines;
}

void Tracker::Start(u64 id) {
    program_id = id;
    samples = 0;
    history.clear();
    slow_streak = 0;
    streak_min = 0;
    last_spot = 0;
    spots = 0;
}

void Tracker::Stop() {
    program_id = 0;
}

QString Tracker::Sample(double fps, double target) {
    if (program_id == 0 || fps < 1.0) {
        slow_streak = 0; // Paused or loading
        return {};
    }
    ++samples;
    // The usual frame rate: what the game reaches most of the time lately (the 80th percentile),
    // unless the target is known.
    double usual = target;
    if (usual <= 0 && history.size() >= 20) {
        std::vector<double> sorted = history;
        std::ranges::sort(sorted);
        usual = sorted[sorted.size() * 8 / 10];
    }
    const bool slow = samples > WarmupSamples && usual > 0 && fps < usual * SlowRatio;
    if (!slow) {
        history.push_back(fps);
        if (history.size() > HistorySize) {
            history.erase(history.begin());
        }
        slow_streak = 0;
        return {};
    }
    streak_min = slow_streak == 0 ? fps : std::min(streak_min, fps);
    if (++slow_streak < StreakSamples) {
        return {};
    }
    slow_streak = 0;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - last_spot < SpotGapSeconds || spots >= MaxSpotsPerSession) {
        return {};
    }
    last_spot = now;
    ++spots;

    auto list = List(program_id);
    Spot spot;
    spot.when = now;
    spot.fps = streak_min;
    spot.target = usual;
    spot.image = Folder(program_id) +
                 QStringLiteral("/%1.jpg")
                     .arg(QDateTime::fromSecsSinceEpoch(now).toString(
                         QStringLiteral("yyyy-MM-dd_HH-mm-ss")));
    list.insert(list.begin(), spot);
    while (list.size() > static_cast<size_t>(MaxKept)) {
        QFile::remove(list.back().image);
        list.pop_back();
    }
    Save(program_id, list);
    return spot.image;
}

void ShowDialog(QWidget* parent, u64 program_id, const QString& title) {
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("FenixRoot"));
    dialog.setWindowTitle(QObject::tr("Slow Spots"));
    dialog.setStyleSheet(Theme::StyleSheet() + QStringLiteral(R"(
QDialog#FenixRoot { background: #121216; }
QLabel#FenixLeaveTitle { color: #ffffff; font-size: 24px; font-weight: 800; }
QListWidget { background: transparent; border: none; color: #f4f4f6; font-size: 13px; outline: none; }
QListWidget::item { border-radius: 12px; padding: 6px; }
QListWidget::item:selected { background: rgba(230, 0, 18, 70); }
)"));
    dialog.resize(980, 680);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(10);
    auto* heading = new QLabel(QObject::tr("Slow Spots"), &dialog);
    heading->setObjectName(QStringLiteral("FenixLeaveTitle"));
    layout->addWidget(heading);
    auto* about = new QLabel(
        QObject::tr("%1\nWhere the game ran well under its usual frame rate for a few seconds, "
                    "with a picture of the moment. Send these with a debug report to find out "
                    "why.")
            .arg(title),
        &dialog);
    about->setObjectName(QStringLiteral("FenixMuted"));
    about->setWordWrap(true);
    layout->addWidget(about);

    auto* list = new QListWidget(&dialog);
    list->setViewMode(QListView::IconMode);
    list->setIconSize(QSize(288, 162));
    list->setGridSize(QSize(308, 220));
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setWordWrap(true);
    layout->addWidget(list, 1);
    const auto fill = [list, program_id] {
        list->clear();
        for (const Spot& spot : List(program_id)) {
            QPixmap picture(spot.image);
            if (picture.isNull()) {
                picture = QPixmap(288, 162);
                picture.fill(QColor(0x22, 0x22, 0x2a));
                QPainter painter(&picture);
                painter.setPen(Theme::TextMuted);
                painter.drawText(picture.rect(), Qt::AlignCenter,
                                 QObject::tr("No picture"));
            } else {
                picture = picture.scaled(288, 162, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            auto* item = new QListWidgetItem(
                QIcon(picture),
                QObject::tr("%1 of %2 FPS\n%3")
                    .arg(spot.fps, 0, 'f', 0)
                    .arg(spot.target, 0, 'f', 0)
                    .arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(spot.when),
                                            QLocale::ShortFormat)));
            item->setData(Qt::UserRole, spot.image);
            list->addItem(item);
        }
        if (list->count() == 0) {
            auto* item = new QListWidgetItem(QObject::tr("No slow spots yet. Play for a while: "
                                                         "they show up here when the game slows "
                                                         "down."));
            item->setFlags(Qt::NoItemFlags);
            list->addItem(item);
        }
    };
    fill();
    QObject::connect(list, &QListWidget::itemActivated, &dialog, [](QListWidgetItem* item) {
        const QString image = item->data(Qt::UserRole).toString();
        if (!image.isEmpty() && QFile::exists(image)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(image));
        }
    });

    auto* buttons = new QHBoxLayout;
    auto* open_folder = new QPushButton(QObject::tr("Open Folder"), &dialog);
    QObject::connect(open_folder, &QPushButton::clicked, &dialog, [program_id] {
        QDir{}.mkpath(Folder(program_id));
        QDesktopServices::openUrl(QUrl::fromLocalFile(Folder(program_id)));
    });
    auto* clear = new QPushButton(QObject::tr("Clear"), &dialog);
    QObject::connect(clear, &QPushButton::clicked, &dialog, [&dialog, program_id, fill] {
        if (QMessageBox::question(&dialog, QObject::tr("Clear slow spots"),
                                  QObject::tr("Delete this game's slow spots and pictures?")) !=
            QMessageBox::Yes) {
            return;
        }
        QDir{Folder(program_id)}.removeRecursively();
        fill();
    });
    auto* close = new QPushButton(QObject::tr("Close"), &dialog);
    close->setObjectName(QStringLiteral("FenixPrimary"));
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addWidget(open_folder);
    buttons->addWidget(clear);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    list->setFocus();
    dialog.exec();
}

} // namespace Fenix::SlowSpots
