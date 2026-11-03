#include "sharedvideo.hpp"

#include <qmetaobject.h>
#include <qtimer.h>

#include <chrono>
#include <cstring>
#include <utility>

extern "C" {
#include <libavutil/cpu.h>
}

namespace caelestia::images {

namespace {

using namespace std::chrono_literals;

constexpr auto k_releaseDelay = 3s;
constexpr auto k_idleRelease = 30s;

QVideoSink* sinkOf(QObject* output) {
    return output ? output->property("videoSink").value<QVideoSink*>() : nullptr;
}

// A copy in system memory, so showing it doesn't keep the decoder that made the frame alive
QVideoFrame detached(QVideoFrame frame) {
    if (!frame.map(QVideoFrame::ReadOnly))
        return {};

    QVideoFrame copy(frame.surfaceFormat());
    if (copy.map(QVideoFrame::WriteOnly)) {
        for (int i = 0; i < frame.planeCount(); ++i) {
            const int rows = std::min(
                frame.mappedBytes(i) / frame.bytesPerLine(i), copy.mappedBytes(i) / copy.bytesPerLine(i));
            const auto len = static_cast<size_t>(std::min(frame.bytesPerLine(i), copy.bytesPerLine(i)));
            for (int y = 0; y < rows; ++y)
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
    // Frames keep the decoder that made them alive, and the last one to go tears it down (~80ms). Outputs can keep
    // showing the old source's last frame for a moment (like the wallpaper crossfading), so hold it until they're
    // likely done and drop it on the decoder thread, rather than leave the teardown to the render thread
    if (m_frame.isValid()) {
        onDecoderThread([this, frame = std::exchange(m_frame, {})] {
            QTimer::singleShot(k_releaseDelay, &m_context, [frame]() mutable {
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
        onDecoderThread([this, source, generation = m_generation] {
            if (!m_player)
                createPlayer();
            m_resumePosition = -1;
            m_player->setSource(source);
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

    const bool play = !m_outputs.isEmpty();
    if (play)
        m_idleTimer.stop();
    else if (!m_idleTimer.isActive())
        m_idleTimer.start();

    onDecoderThread([this, play, source = m_source, generation = m_generation] {
        m_playing = play;
        // Released while idle, so pick up from the same spot
        if (play && !m_player) {
            createPlayer();
            m_player->setSource(source);
            m_decodingGeneration = generation;
        }
        applyPlayback();
    });
}

void SharedVideo::releaseIdle() {
    if (!m_outputs.isEmpty() || m_source.isEmpty())
        return;

    // Removed outputs still show the last frame, which would keep the decoder alive, so swap in a copy
    if (auto copy = detached(m_frame); copy.isValid()) {
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
    QObject::connect(m_player.get(), &QMediaPlayer::mediaStatusChanged, m_player.get(),
        [this](QMediaPlayer::MediaStatus status) {
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

} // namespace caelestia::images
