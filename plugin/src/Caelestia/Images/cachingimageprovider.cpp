#include "cachingimageprovider.hpp"

#include <qcoreapplication.h>
#include <qfileinfo.h>
#include <qimage.h>
#include <qimagereader.h>
#include <qloggingcategory.h>
#include <qmutex.h>
#include <qpainter.h>
#include <qthreadpool.h>

#include <memory>
#include <utility>

#include "imagecacher.hpp"

namespace {

Q_LOGGING_CATEGORY(lcCProv, "caelestia.images.cacheprovider", QtInfoMsg)

} // namespace

namespace caelestia::images {

using Qt::StringLiterals::operator""_s;

namespace {

struct DecodePool {
    DecodePool() {
        pool.setMaxThreadCount(4);
        // Decoding uses QtGui, which is torn down after quitting, so drop queued decodes and finish running ones first
        auto* const app = QCoreApplication::instance();
        QObject::connect(app, &QCoreApplication::aboutToQuit, app, [this] {
            pool.clear();
            pool.waitForDone();
        });
    }

    QThreadPool pool;
};

QThreadPool& decodePool() {
    static DecodePool s_pool;
    return s_pool.pool;
}

struct DecodeResult {
    QImage image;
    QString error;
};

DecodeResult decode(const QString& id, const QSize& requestedSize, ImageCacher::FillMode fillMode);

class CachingImageResponse final : public QQuickImageResponse {
public:
    CachingImageResponse(const QString& id, const QSize& requestedSize, ImageCacher::FillMode fillMode);
    ~CachingImageResponse() override;

    [[nodiscard]] QQuickTextureFactory* textureFactory() const override;

    [[nodiscard]] QString errorString() const override;

private:
    // The engine deletes responses it no longer needs (on reload or shutdown) even while their decode is running, so
    // the decode job only reaches the response through this link, which the response clears when it is destroyed
    struct Link {
        QMutex mutex;
        CachingImageResponse* response = nullptr;
    };

    std::shared_ptr<Link> m_link = std::make_shared<Link>();
    DecodeResult m_result;
};

CachingImageResponse::CachingImageResponse(
    const QString& id, const QSize& requestedSize, ImageCacher::FillMode fillMode) {
    m_link->response = this;
    decodePool().start([link = m_link, id, requestedSize, fillMode] {
        DecodeResult result = decode(id, requestedSize, fillMode);

        const QMutexLocker locker(&link->mutex);
        if (auto* const response = link->response) {
            // Queued, so the call is dropped if the response is deleted before it runs
            QMetaObject::invokeMethod(
                response,
                [response, result = std::move(result)] {
                    response->m_result = result;
                    emit response->finished();
                },
                Qt::QueuedConnection);
        }
    });
}

CachingImageResponse::~CachingImageResponse() {
    const QMutexLocker locker(&m_link->mutex);
    m_link->response = nullptr;
}

QQuickTextureFactory* CachingImageResponse::textureFactory() const {
    return QQuickTextureFactory::textureFactoryForImage(m_result.image);
}

QString CachingImageResponse::errorString() const {
    return m_result.error;
}

DecodeResult decode(const QString& id, const QSize& requestedSize, ImageCacher::FillMode fillMode) {
    DecodeResult result;
    QString path = QString::fromUtf8(id.toUtf8().percentDecoded());
    if (!path.startsWith(u'/'))
        path.prepend(u'/');

    if (!QFileInfo::exists(path)) {
        result.error = u"Source file does not exist: "_s + path;
        qCWarning(lcCProv).noquote() << result.error;
        return result;
    }

    QSize size = requestedSize;
    const bool needsW = size.width() <= 0;
    const bool needsH = size.height() <= 0;

    // If both dimensions are missing, return the original directly
    if (needsW && needsH) {
        qCDebug(lcCProv).noquote() << "Given source size is invalid, returning original:" << path;
        result.image = QImage(path);
        if (result.image.isNull()) {
            result.error = u"Failed to decode source: "_s + path;
            qCWarning(lcCProv).noquote() << result.error;
        }
        return result;
    }

    // If one dimension is missing, derive it from the source aspect ratio
    if (needsW || needsH) {
        const QImageReader sourceReader(path);
        const QSize sourceSize = sourceReader.size();
        if (!sourceSize.isValid() || sourceSize.isEmpty()) {
            result.error = u"Could not determine source size for: "_s + path;
            qCWarning(lcCProv).noquote() << result.error;
            return result;
        }

        if (needsW)
            size.setWidth(qRound(size.height() * sourceSize.width() / static_cast<qreal>(sourceSize.height())));
        else
            size.setHeight(qRound(size.width() * sourceSize.height() / static_cast<qreal>(sourceSize.width())));
    }

    // Only small images (thumbnails, avatars) use the lossy disk cache. Full-screen images like the wallpaper
    // are decoded straight from the source instead, so they never lose quality to a re-encode
    constexpr qint64 k_maxCachedArea = 1024 * 1024;
    const bool useCache = static_cast<qint64>(size.width()) * size.height() <= k_maxCachedArea;

    // Try to use cached image
    const auto cachePath = useCache ? ImageCacher::cachePathFor(path, size, fillMode) : QString();
    if (!cachePath.isEmpty()) {
        QImageReader cacheReader(cachePath);
        if (cacheReader.canRead()) {
            result.image = cacheReader.read();
            if (!result.image.isNull())
                return result;
        }
    }

    QImageReader sourceReader(path);
    const QSize sourceSize = sourceReader.size();
    if (!sourceSize.isValid() || sourceSize.isEmpty()) {
        result.error = u"Could not determine source size for: "_s + path;
        qCWarning(lcCProv).noquote() << result.error;
        return result;
    }

    Qt::AspectRatioMode scaleMode;
    switch (fillMode) {
    case ImageCacher::FillMode::Crop:
        scaleMode = Qt::KeepAspectRatioByExpanding;
        break;
    case ImageCacher::FillMode::Fit:
        scaleMode = Qt::KeepAspectRatio;
        break;
    case ImageCacher::FillMode::Stretch:
        scaleMode = Qt::IgnoreAspectRatio;
        break;
    }

    sourceReader.setScaledSize(sourceSize.scaled(size, scaleMode));
    QImage image = sourceReader.read();
    if (image.isNull()) {
        result.error = u"Failed to decode source: "_s + path + u": "_s + sourceReader.errorString();
        qCWarning(lcCProv).noquote() << result.error;
        return result;
    }

    if (fillMode == ImageCacher::FillMode::Stretch) {
        result.image = image;
    } else {
        result.image = QImage(size, QImage::Format_ARGB32);
        result.image.fill(Qt::transparent);

        QPainter painter(&result.image);
        painter.drawImage((size.width() - image.width()) / 2, (size.height() - image.height()) / 2, image);
    }

    ImageCacher::instance()->schedule(cachePath, result.image);
    return result;
}

} // namespace

CachingImageProvider::CachingImageProvider(FillMode fillMode)
    : m_fillMode(fillMode) {}

QQuickImageResponse* CachingImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize) {
    return new CachingImageResponse(id, requestedSize, m_fillMode);
}

} // namespace caelestia::images
