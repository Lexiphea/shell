#include "sharedvideo.hpp"

#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qguiapplication.h>
#include <qloggingcategory.h>
#include <qmetaobject.h>
#include <qscreen.h>
#include <qtimer.h>

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <utility>

extern "C" {
#include <libavutil/cpu.h>
}

#include "imagecacher.hpp"

namespace {

Q_LOGGING_CATEGORY(lcVideo, "caelestia.images.video", QtInfoMsg)

} // namespace

namespace caelestia::shell::images {

namespace {

using Qt::StringLiterals::operator""_s;
using namespace std::chrono_literals;

constexpr auto k_releaseDelay = 3s;
constexpr auto k_idleRelease = 30s;
// Wait for the source to settle before making a copy, so browsing wallpapers doesn't encode each one
constexpr auto k_proxyDelay = 5s;
// Only copy videos with at least ~1.5x the pixels needed, below that the saving isn't worth a lossy re-encode
constexpr qreal k_maxProxyScale = 0.8;

// The smallest size that covers every screen
QSize screensBox() {
    QSize box;
    const auto screens = QGuiApplication::screens();
    for (const QScreen* const screen : screens)
        box = box.expandedTo(screen->size() * screen->devicePixelRatio());
    return box;
}

// The screen-sized copy once it has been made, otherwise the source itself
QUrl playable(const QUrl& source, const QString& proxyPath) {
    return !proxyPath.isEmpty() && QFile::exists(proxyPath) ? QUrl::fromLocalFile(proxyPath) : source;
}

QVideoSink* sinkOf(const QObject* output) {
    return output ? output->property("videoSink").value<QVideoSink*>() : nullptr;
}

// A copy in system memory, so showing it doesn't keep the decoder that made the frame alive
QVideoFrame detached(QVideoFrame frame) {
    if (!frame.map(QVideoFrame::ReadOnly))
        return {};

    QVideoFrame copy(frame.surfaceFormat());
    if (copy.map(QVideoFrame::WriteOnly)) {
        for (int i = 0; i < frame.planeCount(); ++i) {
            const int rows =
                std::min(frame.mappedBytes(i) / frame.bytesPerLine(i), copy.mappedBytes(i) / copy.bytesPerLine(i));
            const auto len = static_cast<size_t>(std::min(frame.bytesPerLine(i), copy.bytesPerLine(i)));
            for (ptrdiff_t y = 0; y < rows; ++y)
                std::memcpy(copy.bits(i) + y * copy.bytesPerLine(i), frame.bits(i) + y * frame.bytesPerLine(i), len);
        }
        copy.unmap();
    }
    frame.unmap();
    return copy;
}

} // namespace

SharedVideo::SharedVideo(QObject* parent)
    : QObject(parent) {
    m_thread.setObjectName(QStringLiteral("SharedVideo"));
    // Qt opens decoders with threads=auto, one per core plus one. For H.264 each frame thread keeps its own tables and
    // pictures (~10 MB per thread at 4K), though with hardware decoding the GPU does the work. Three threads keep
    // that small and still decode 4K60 in software when there's no hardware decoder. FFmpeg in this process is only
    // Qt Multimedia, so the cap applies nowhere else
    av_cpu_force_count(2);
    m_context.moveToThread(&m_thread);

    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(k_idleRelease);
    connect(&m_idleTimer, &QTimer::timeout, this, &SharedVideo::releaseIdle);

    m_proxyTimer.setSingleShot(true);
    m_proxyTimer.setInterval(k_proxyDelay);
    connect(&m_proxyTimer, &QTimer::timeout, this, &SharedVideo::makeProxy);
}

SharedVideo::~SharedVideo() {
    if (!m_thread.isRunning())
        return;

    // Quit from the cleanup itself, as quit() would skip calls still queued
    onDecoderThread([this] {
        releasePlayer();
        m_thread.quit();
    });
    m_thread.wait();
}

QUrl SharedVideo::source() const {
    return m_source;
}

bool SharedVideo::ready() const {
    return m_ready;
}

void SharedVideo::setSource(const QUrl& source) {
    if (m_source == source)
        return;

    m_source = source;
    ++m_generation;

    updateProxyPath();
    // Frames keep the decoder that made them alive, and the last one to go tears it down (~80ms). Outputs can keep
    // showing the old source's last frame for a moment (like the wallpaper crossfading), so hold it until they're
    // likely done and drop it on the decoder thread, rather than leave the teardown to the render thread
    if (m_frame.isValid()) {
        onDecoderThread([this, frame = std::exchange(m_frame, {})] {
            QTimer::singleShot(k_releaseDelay, &m_context, [frame] mutable {
                frame = {};
            });
        });
    }
    if (source.isEmpty()) {
        onDecoderThread([this] {
            releasePlayer();
            m_resumePosition = -1;
        });
        // The outputs keep their own copy of the last frame
        for (const auto& output : std::as_const(m_outputs)) {
            if (output)
                output->setVideoFrame({});
        }
    } else {
        onDecoderThread([this, source, proxyPath = m_proxyPath, generation = m_generation] {
            if (!m_player)
                createPlayer();
            m_resumePosition = -1;
            m_player->setSource(playable(source, proxyPath));
            // After setSource, which stops the old source's frames
            m_decodingGeneration = generation;
        });
    }
    emit sourceChanged();
    updatePlayback();
}

void SharedVideo::preload() {
    // The first sink loads the backend, which probes the hardware decoders
    onDecoderThread([this] {
        [[maybe_unused]] const QVideoSink sink;
        backendLoaded();
    });
}

void SharedVideo::addOutput(QObject* output) {
    auto* const sink = sinkOf(output);
    if (!sink || m_outputs.contains(sink))
        return;

    m_outputs << sink;
    if (!m_viewers.contains(sink))
        m_viewers << sink;
    // Show the current frame straight away rather than waiting for the next one
    if (m_frame.isValid())
        sink->setVideoFrame(m_frame);
    updatePlayback();
}

void SharedVideo::removeOutput(QObject* output) {
    m_outputs.removeAll(sinkOf(output));
    updatePlayback();
}

void SharedVideo::updatePlayback() {
    m_outputs.removeAll(nullptr);
    if (m_source.isEmpty())
        return;

    // Screens may have changed since the source was set
    updateProxyPath();

    const bool play = !m_outputs.isEmpty();
    if (play)
        m_idleTimer.stop();
    else if (!m_idleTimer.isActive())
        m_idleTimer.start();

    onDecoderThread([this, play, source = m_source, proxyPath = m_proxyPath, generation = m_generation] {
        m_playing = play;
        // Released while idle, so pick up from the same spot. A copy made since is used from here on
        if (play && !m_player) {
            createPlayer();
            m_player->setSource(playable(source, proxyPath));
            m_decodingGeneration = generation;
        } else if (m_player && m_player->source() != source && m_player->source() != QUrl::fromLocalFile(proxyPath)) {
            // Playing a copy made for other screens, which may no longer cover them, so switch at the same spot
            m_resumePosition = m_player->position();
            m_player->setSource(playable(source, proxyPath));
        }
        applyPlayback();
    });
}

void SharedVideo::releaseIdle() {
    if (!m_outputs.isEmpty() || m_source.isEmpty())
        return;

    // Removed outputs still show the last frame, which would keep the decoder alive, so swap in a copy
    if (const auto copy = detached(m_frame); copy.isValid()) {
        m_frame = copy;
        m_viewers.removeAll(nullptr);
        for (const auto& sink : std::as_const(m_viewers))
            sink->setVideoFrame(m_frame);
    }

    onDecoderThread([this] {
        if (!m_player)
            return;
        // Still loading after a previous release, so its position isn't known yet
        if (m_resumePosition < 0)
            m_resumePosition = m_player->position();
        releasePlayer();
    });
}

void SharedVideo::updateProxyPath() {
    const QSize box = screensBox();
    const QString path = m_source.isLocalFile() ? ImageCacher::cachePathFor(QFileInfo(m_source.toLocalFile()), box,
                                                      ImageCacher::FillMode::Crop, u"mp4")
                                                : QString();
    if (path == m_proxyPath)
        return;

    m_proxyJob = nullptr; // Stops encoding for the previous source or screens
    m_proxyTimer.stop();
    m_proxyBox = box;
    m_proxyPath = path;
    m_proxyPending = !path.isEmpty();
}

void SharedVideo::makeProxy() {
    // Only 8-bit 4:2:0 is copied, as anything else would lose precision in the copy
    const auto format = m_frame.pixelFormat();
    if (m_proxyPath.isEmpty() || QFile::exists(m_proxyPath) || !m_frame.isValid() ||
        (format != QVideoFrameFormat::Format_NV12 && format != QVideoFrameFormat::Format_YUV420P))
        return;

    const QSize frame = m_frame.size();
    const qreal scale = std::max(m_proxyBox.width() / static_cast<qreal>(frame.width()),
        m_proxyBox.height() / static_cast<qreal>(frame.height()));
    if (scale > k_maxProxyScale)
        return;

    // Even sizes, as 4:2:0 needs them
    const auto even = [scale](int length) {
        return static_cast<int>(std::ceil(length * scale / 2)) * 2;
    };
    const QString part = m_proxyPath + u".part"_s;
    if (!QDir().mkpath(QFileInfo(part).absolutePath()))
        return;

    m_proxyJob = std::make_unique<QProcess>();
    // In the background, so encoding never competes with anything on screen
    m_proxyJob->setChildProcessModifier([] {
        static_cast<void>(nice(19));
    });
    connect(m_proxyJob.get(), &QProcess::finished, this,
        [job = m_proxyJob.get(), part, path = m_proxyPath](int code, QProcess::ExitStatus status) {
            if (status == QProcess::NormalExit && code == 0 && QFile::rename(part, path)) {
                qCDebug(lcVideo).noquote() << "Made screen-sized copy" << path;
                // Only the latest copy is kept, so they never take more disk than one video
                QDir dir(ImageCacher::cacheDir());
                const QStringList copies = dir.entryList({ u"*.mp4"_s }, QDir::Files);
                for (const QString& name : copies) {
                    if (dir.filePath(name) != path)
                        dir.remove(name);
                }
                return;
            }
            QFile::remove(part);
            qCWarning(lcVideo).noquote() << "Failed to make a screen-sized copy:"
                                         << QString::fromUtf8(job->readAllStandardError()).trimmed();
        });
    // Lanczos and CRF 18, x264's usual visually lossless point, keep the copy indistinguishable from the source scaled
    // on the GPU (SSIM ~0.99) at ~40% of a 4K source's size
    m_proxyJob->start(
        u"ffmpeg"_s, {
                         u"-nostdin"_s,
                         u"-v"_s,
                         u"error"_s,
                         u"-y"_s,
                         u"-i"_s,
                         m_source.toLocalFile(),
                         u"-vf"_s,
                         u"scale=%1:%2:flags=lanczos"_s.arg(even(frame.width())).arg(even(frame.height())),
                         u"-c:v"_s,
                         u"libx264"_s,
                         u"-preset"_s,
                         u"medium"_s,
                         u"-crf"_s,
                         u"18"_s,
                         u"-pix_fmt"_s,
                         u"yuv420p"_s,
                         u"-an"_s,
                         u"-movflags"_s,
                         u"+faststart"_s,
                         u"-f"_s,
                         u"mp4"_s,
                         part,
                     });
}

void SharedVideo::createPlayer() {
    m_sink = std::make_unique<QVideoSink>();
    // Tag each frame with its source as it's decoded, then forward it on the GUI thread, where the outputs live
    QObject::connect(
        m_sink.get(), &QVideoSink::videoFrameChanged, m_sink.get(),
        [this](const QVideoFrame& frame) {
            QMetaObject::invokeMethod(this, [this, frame, generation = m_decodingGeneration.load()] {
                if (generation != m_generation)
                    return;
                m_frame = frame;
                // The first frame gives the size and format the copy depends on
                if (std::exchange(m_proxyPending, false))
                    m_proxyTimer.start();
                for (const auto& output : std::as_const(m_outputs)) {
                    if (output)
                        output->setVideoFrame(frame);
                }
            });
        },
        Qt::DirectConnection);
    m_player = std::make_unique<QMediaPlayer>();
    m_player->setLoops(QMediaPlayer::Infinite);
    m_player->setVideoSink(m_sink.get());
    // Seeking or playing while the media loads would be ignored or start from the beginning, so resume once loaded
    QObject::connect(
        m_player.get(), &QMediaPlayer::mediaStatusChanged, m_player.get(), [this](QMediaPlayer::MediaStatus status) {
            if (status != QMediaPlayer::LoadedMedia || m_resumePosition < 0)
                return;
            m_player->setPosition(std::exchange(m_resumePosition, -1));
            applyPlayback();
        });
    backendLoaded();
}

void SharedVideo::releasePlayer() {
    m_player.reset();
    m_sink.reset();
}

void SharedVideo::applyPlayback() {
    if (!m_player || m_resumePosition >= 0)
        return;

    if (m_playing)
        m_player->play();
    else
        m_player->pause();
}

void SharedVideo::backendLoaded() {
    QMetaObject::invokeMethod(this, [this] {
        if (!m_ready) {
            m_ready = true;
            emit readyChanged();
        }
    });
}

void SharedVideo::onDecoderThread(std::function<void()> fn) {
    m_thread.start(); // Does nothing if already running
    QMetaObject::invokeMethod(&m_context, std::move(fn));
}

} // namespace caelestia::shell::images
