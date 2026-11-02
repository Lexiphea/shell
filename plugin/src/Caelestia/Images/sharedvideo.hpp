#pragma once

#include <qlist.h>
#include <qmediaplayer.h>
#include <qobject.h>
#include <qpointer.h>
#include <qqmlintegration.h>
#include <qthread.h>
#include <qurl.h>
#include <qvideoframe.h>
#include <qvideosink.h>

#include <atomic>
#include <functional>
#include <memory>

namespace caelestia::images {

// Loops one video into any number of QML VideoOutputs, so each frame is decoded once however many show it. Plays only
// while at least one output is added. The player and its sink exist only while there's a source: the first sink takes
// ~0.8s to load the media backend, which image wallpapers shouldn't pay at startup. They live on their own thread,
// since every new source sets up a hardware decoder (~70ms) on the player's thread, which would stall the UI
class SharedVideo : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    // Whether the media backend is loaded. Creating a VideoOutput before then would load it on the GUI thread instead
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit SharedVideo(QObject* parent = nullptr);
    ~SharedVideo() override;

    [[nodiscard]] QUrl source() const;
    void setSource(const QUrl& source);

    [[nodiscard]] bool ready() const;

    // Loads the media backend on the decoder thread ahead of time, so the first video doesn't stall the UI for ~0.8s
    Q_INVOKABLE void preload();
    // Takes a QML VideoOutput
    Q_INVOKABLE void addOutput(QObject* output);
    Q_INVOKABLE void removeOutput(QObject* output);

signals:
    void sourceChanged();
    void readyChanged();

private:
    void updatePlayback();
    // On m_thread
    void createPlayer();
    void releasePlayer();
    void backendLoaded();
    void onDecoderThread(std::function<void()> fn);

    QUrl m_source;
    bool m_ready = false;
    QVideoFrame m_frame;
    quint64 m_generation = 0; // Bumped on every source change, so frames of the previous source are dropped
    QList<QPointer<QVideoSink>> m_outputs;

    QThread m_thread;
    QObject m_context; // Lives on m_thread, to queue calls there
    // Only touched on m_thread
    std::unique_ptr<QVideoSink> m_sink;
    std::unique_ptr<QMediaPlayer> m_player;
    std::atomic<quint64> m_decodingGeneration = 0; // The generation the player is decoding, read as frames arrive
};

} // namespace caelestia::images
