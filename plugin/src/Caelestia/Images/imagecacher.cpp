#include "imagecacher.hpp"

#include <qcoreapplication.h>
#include <qcryptographichash.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qloggingcategory.h>
#include <qmutex.h>
#include <qsavefile.h>
#include <qthreadpool.h>

#include <algorithm>

namespace {

Q_LOGGING_CATEGORY(lcCacher, "caelestia.images.cacher", QtInfoMsg)

} // namespace

namespace caelestia::shell::images {

using Qt::StringLiterals::operator""_s;

namespace {

struct CacheWritePool {
    CacheWritePool() {
        pool.setMaxThreadCount(2);
        // Encoding uses QtGui, which is torn down after quitting, so finish pending writes first
        auto* const app = QCoreApplication::instance();
        QObject::connect(app, &QCoreApplication::aboutToQuit, app, [this] {
            pool.waitForDone();
        });
    }

    QThreadPool pool;
};

QThreadPool& cacheWritePool() {
    static CacheWritePool s_pool;
    return s_pool.pool;
}

bool isOpaque(const QImage& image) {
    if (!image.hasAlphaChannel())
        return true;
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < argb.height(); ++y) {
        const auto* const row = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        if (!std::all_of(row, row + argb.width(), [](QRgb pixel) {
                return qAlpha(pixel) == 255;
            }))
            return false;
    }
    return true;
}

QString fillSuffix(ImageCacher::FillMode fillMode) {
    switch (fillMode) {
    case ImageCacher::FillMode::Crop:
        return u"crop"_s;
    case ImageCacher::FillMode::Fit:
        return u"fit"_s;
    default:
        return u"stretch"_s;
    }
}

} // namespace

const QString& ImageCacher::cacheDir() {
    static const QString k_dir = [] {
        QString cache = qEnvironmentVariable("XDG_CACHE_HOME");
        if (cache.isEmpty())
            cache = QDir::homePath() + u"/.cache"_s;
        return cache + u"/caelestia/imagecache"_s;
    }();
    return k_dir;
}

QString ImageCacher::cachePathFor(
    const QFileInfo& source, const QSize& size, FillMode fillMode, QStringView extension) {
    const QString canonicalPath = source.canonicalFilePath();
    if (canonicalPath.isEmpty())
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(canonicalPath.toUtf8());
    hash.addData(QByteArrayView("\0", 1));
    hash.addData(QByteArray::number(source.size()));
    hash.addData(QByteArrayView("\0", 1));
    hash.addData(QByteArray::number(source.lastModified().toMSecsSinceEpoch()));

    const QString filename = u"%1@%2x%3-%4.%5"_s.arg(QString::fromLatin1(hash.result().toHex()),
        QString::number(size.width()), QString::number(size.height()), fillSuffix(fillMode), extension);

    return cacheDir() + u'/' + filename;
}

ImageCacher* ImageCacher::instance() {
    static ImageCacher s_instance;
    return &s_instance;
}

ImageCacher::ImageCacher(QObject* parent)
    : QObject(parent) {
    // Clear out files left from the old PNG and WebP-only formats
    cacheWritePool().start([] {
        QDir dir(cacheDir());
        const QStringList old = dir.entryList({ u"*.png"_s, u"*.webp"_s }, QDir::Files);
        for (const QString& name : old)
            dir.remove(name);
    });
}

void ImageCacher::schedule(const QString& cachePath, const QImage& image) {
    if (cachePath.isEmpty())
        return;

    {
        const QMutexLocker locker(&m_mutex);
        if (m_inflight.contains(cachePath))
            return;
        m_inflight.insert(cachePath);
    }

    cacheWritePool().start([this, cachePath, image] {
        runJob(cachePath, image);
        const QMutexLocker locker(&m_mutex);
        // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage) m_inflight is a value member, not a pointer
        m_inflight.remove(cachePath);
    });
}

void ImageCacher::runJob(const QString& cachePath, const QImage& image) {
    if (QFile::exists(cachePath))
        return;

    const QString parent = QFileInfo(cachePath).absolutePath();
    if (!QDir().mkpath(parent)) {
        qCWarning(lcCacher).noquote() << "Failed to create cache dir" << parent;
        return;
    }

    // Opaque images are stored as JPEG, which reads ~2.5x and writes ~30x faster than WebP for ~25% more disk. Readers
    // tell the formats apart by content
    QSaveFile saveFile(cachePath);
    const char* const format = isOpaque(image) ? "JPEG" : "WEBP";
    if (!saveFile.open(QIODevice::WriteOnly) || !image.save(&saveFile, format, 90) || !saveFile.commit()) {
        qCWarning(
            lcCacher, "Failed to save to %s: %s", qUtf8Printable(cachePath), qUtf8Printable(saveFile.errorString()));
        return;
    }

    qCDebug(lcCacher).noquote() << "Saved to" << cachePath;
}

} // namespace caelestia::shell::images
