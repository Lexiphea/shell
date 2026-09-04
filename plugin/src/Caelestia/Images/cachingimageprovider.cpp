#include "cachingimageprovider.hpp"

#include "imagecacher.hpp"

#include <qfileinfo.h>
#include <qimage.h>
#include <qimagereader.h>
#include <qloggingcategory.h>
#include <qpainter.h>
#include <qrunnable.h>
#include <qthreadpool.h>

Q_LOGGING_CATEGORY(lcCProv, "caelestia.images.cacheprovider", QtInfoMsg)

namespace caelestia::images {

namespace {

struct DecodePool {
    DecodePool() { pool.setMaxThreadCount(4); }

    QThreadPool pool;
};

QThreadPool& decodePool() {
    static DecodePool s_pool;
    return s_pool.pool;
}

class CachingImageResponse final : public QQuickImageResponse, public QRunnable {
public:
    CachingImageResponse(const QString& id, const QSize& requestedSize, ImageCacher::FillMode fillMode)
        : m_id(id)
        , m_requestedSize(requestedSize)
        , m_fillMode(fillMode) {
        setAutoDelete(false);
    }

    [[nodiscard]] QQuickTextureFactory* textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    [[nodiscard]] QString errorString() const override { return m_error; }

    void run() override {
        process();
        emit finished();
    }

private:
    void process() {
        QString path = QString::fromUtf8(m_id.toUtf8().percentDecoded());
        if (!path.startsWith(QLatin1Char('/')))
            path.prepend(QLatin1Char('/'));

        if (!QFileInfo::exists(path)) {
            m_error = QStringLiteral("Source file does not exist: ") + path;
            qCWarning(lcCProv).noquote() << m_error;
            return;
        }

        QSize size = m_requestedSize;
        const bool needsW = size.width() <= 0;
        const bool needsH = size.height() <= 0;

        // If both dimensions are missing, return the original directly
        if (needsW && needsH) {
            qCDebug(lcCProv).noquote() << "Given source size is invalid, returning original:" << path;
            m_image = QImage(path);
            if (m_image.isNull()) {
                m_error = QStringLiteral("Failed to decode source: ") + path;
                qCWarning(lcCProv).noquote() << m_error;
            }
            return;
        }

        // If one dimension is missing, derive it from the source aspect ratio
        if (needsW || needsH) {
            const QImageReader sourceReader(path);
            const QSize sourceSize = sourceReader.size();
            if (!sourceSize.isValid() || sourceSize.isEmpty()) {
                m_error = QStringLiteral("Could not determine source size for: ") + path;
                qCWarning(lcCProv).noquote() << m_error;
                return;
            }

            if (needsW)
                size.setWidth(qRound(size.height() * sourceSize.width() / static_cast<qreal>(sourceSize.height())));
            else
                size.setHeight(qRound(size.width() * sourceSize.height() / static_cast<qreal>(sourceSize.width())));
        }

        // Try to use cached image
        const auto cachePath = ImageCacher::cachePathFor(path, size, m_fillMode);
        if (!cachePath.isEmpty()) {
            QImageReader cacheReader(cachePath);
            if (cacheReader.canRead()) {
                m_image = cacheReader.read();
                if (!m_image.isNull())
                    return;
            }
        }

        QImageReader sourceReader(path);
        const QSize sourceSize = sourceReader.size();
        if (!sourceSize.isValid() || sourceSize.isEmpty()) {
            m_error = QStringLiteral("Could not determine source size for: ") + path;
            qCWarning(lcCProv).noquote() << m_error;
            return;
        }

        Qt::AspectRatioMode scaleMode;
        switch (m_fillMode) {
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
            m_error = QStringLiteral("Failed to decode source: ") + path + QStringLiteral(": ") + sourceReader.errorString();
            qCWarning(lcCProv).noquote() << m_error;
            return;
        }

        if (m_fillMode == ImageCacher::FillMode::Stretch) {
            m_image = image;
        } else {
            m_image = QImage(size, QImage::Format_ARGB32);
            m_image.fill(Qt::transparent);

            QPainter painter(&m_image);
            painter.drawImage((size.width() - image.width()) / 2, (size.height() - image.height()) / 2, image);
        }

        ImageCacher::instance()->schedule(cachePath, m_image);
    }

    QString m_id;
    QSize m_requestedSize;
    ImageCacher::FillMode m_fillMode;
    QImage m_image;
    QString m_error;
};

} // namespace

CachingImageProvider::CachingImageProvider(FillMode fillMode)
    : m_fillMode(fillMode) {}

QQuickImageResponse* CachingImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize) {
    auto* const response = new CachingImageResponse(id, requestedSize, m_fillMode);
    decodePool().start(response);
    return response;
}

} // namespace caelestia::images
