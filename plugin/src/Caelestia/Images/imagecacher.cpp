#include "imagecacher.hpp"

#include <qcryptographichash.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qloggingcategory.h>
#include <qmutex.h>
#include <qsavefile.h>
#include <qthreadpool.h>

namespace {

Q_LOGGING_CATEGORY(lcCacher, "caelestia.images.cacher", QtInfoMsg)

} // namespace

namespace caelestia::images {

using Qt::StringLiterals::operator""_s;

namespace {

struct CacheWritePool {
    CacheWritePool() { pool.setMaxThreadCount(2); }

    QThreadPool pool;
};

QThreadPool& cacheWritePool() {
    static CacheWritePool s_pool;
    return s_pool.pool;
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

QString ImageCacher::cachePathFor(const QString& sourcePath, const QSize& size, FillMode fillMode) {
    const QFileInfo info(sourcePath);
    const QString canonicalPath = info.canonicalFilePath();
    if (canonicalPath.isEmpty())
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(canonicalPath.toUtf8());
    hash.addData(QByteArrayView("\0", 1));
    hash.addData(QByteArray::number(info.size()));
    hash.addData(QByteArrayView("\0", 1));
    hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));

    const QString filename = u"%1@%2x%3-%4.webp"_s.arg(QString::fromLatin1(hash.result().toHex()),
        QString::number(size.width()), QString::number(size.height()), fillSuffix(fillMode));

    return cacheDir() + u'/' + filename;
}

ImageCacher* ImageCacher::instance() {
    static ImageCacher s_instance;
    return &s_instance;
}

ImageCacher::ImageCacher(QObject* parent)
    : QObject(parent) {
    // The cache used to be PNG, so clear out any files left from before the switch to WebP
    cacheWritePool().start([] {
        QDir dir(cacheDir());
        const QStringList pngs = dir.entryList({ u"*.png"_s }, QDir::Files);
        for (const QString& name : pngs)
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

    QSaveFile saveFile(cachePath);
    if (!saveFile.open(QIODevice::WriteOnly) || !image.save(&saveFile, "WEBP", 90) || !saveFile.commit()) {
        qCWarning(
            lcCacher, "Failed to save to %s: %s", qUtf8Printable(cachePath), qUtf8Printable(saveFile.errorString()));
        return;
    }

    qCDebug(lcCacher).noquote() << "Saved to" << cachePath;
}

} // namespace caelestia::images
