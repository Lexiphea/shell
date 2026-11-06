#pragma once

#include <qlist.h>
#include <qmediaplayer.h>
#include <qobject.h>
#include <qpointer.h>
#include <qprocess.h>
#include <qqmlintegration.h>
#include <qthread.h>
#include <qtimer.h>
#include <qurl.h>
#include <qvideoframe.h>
#include <qvideosink.h>

#include <atomic>
#include <functional>
#include <memory>

namespace caelestia::shell::images {

// Loops one video into any number of QML VideoOutputs, so each frame is decoded once however many show it. Plays only
// while at least one output is added. The player and its sink exist only while there's a source: the first sink takes
// ~0.8s to load the media backend, which image wallpapers shouldn't pay at startup. They live on their own thread,
// since every new source sets up a hardware decoder (~70ms) on the player's thread, which would stall the UI. After a
// while with no outputs the player is released too, as a paused 4K decoder holds ~280 MB; it resumes where it left off.
// Videos larger than the screens get a copy at screen size, played in place of the source once made: a 4K60 video
// costs ~2x the CPU of a 1440p one, as every frame is copied out of the GPU decoder and uploaded again
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
    void releaseIdle();
    void updateProxyPath();
    void makeProxy();
    // On m_thread
    void createPlayer();
    void releasePlayer();
    void applyPlayback();
    void backendLoaded();
    void onDecoderThread(std::function<void()> fn);

    QUrl m_source;
    bool m_ready = false;
    QVideoFrame m_frame;
    quint64 m_generation = 0; // Bumped on every source change, so frames of the previous source are dropped
    QList<QPointer<QVideoSink>> m_outputs;
    QList<QPointer<QVideoSink>> m_viewers; // Every output given frames, including removed ones still showing the last
    QTimer m_idleTimer;
    QString m_proxyPath;         // Where the screen-sized copy of the source goes
    QSize m_proxyBox;            // The size the copy has to cover
    bool m_proxyPending = false; // Whether to consider a copy once the source's first frame shows its size and format
    QTimer m_proxyTimer;
    std::unique_ptr<QProcess> m_proxyJob;

    QThread m_thread;
    QObject m_context; // Lives on m_thread, to queue calls there
    // Only touched on m_thread
    std::unique_ptr<QVideoSink> m_sink;
    std::unique_ptr<QMediaPlayer> m_player;
    bool m_playing = false;
    qint64 m_resumePosition = -1; // Where to seek once a player recreated after an idle release has loaded
    std::atomic<quint64> m_decodingGeneration = 0; // The generation the player is decoding, read as frames arrive
};

} // namespace caelestia::shell::images
