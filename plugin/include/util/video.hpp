#pragma once

#include <qstring.h>
#include <qstringlist.h>

#include <algorithm>

namespace util {

// Matches Images.validVideoExtensions in QML and VIDEO_EXTENSIONS in the CLI
inline const QStringList& videoExtensions() {
    static const QStringList k_extensions = {
        QStringLiteral("mp4"),
        QStringLiteral("mkv"),
        QStringLiteral("webm"),
        QStringLiteral("mov"),
        QStringLiteral("avi"),
    };
    return k_extensions;
}

inline bool isVideo(const QString& path) {
    return std::ranges::any_of(videoExtensions(), [&path](const QString& extension) {
        return path.endsWith(u'.' + extension, Qt::CaseInsensitive);
    });
}

} // namespace util
