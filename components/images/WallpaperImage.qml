pragma ComponentBehavior: Bound

import QtQuick
import QtMultimedia
import qs.components
import qs.services
import qs.utils

// Crossfades between wallpapers. Videos share one decoder through Wallpapers.video and show their first frame while not playing
Item {
    id: root

    property string source: Wallpapers.current
    property bool playing: true
    property CachingImage current
    property list<CachingImage> images: []
    property bool completed

    function updateImage(): void {
        if (!completed)
            return;

        if (!source) {
            const oldImages = images;
            images = [];
            current = null;
            for (const image of oldImages)
                image.destroy();
            return;
        }

        current = imgComp.createObject(this, {
            path: source
        });
        images = [...images, current];
    }

    onSourceChanged: updateImage()

    Component.onCompleted: {
        completed = true;
        updateImage();
    }

    Component {
        id: imgComp

        CachingImage {
            id: img

            anchors.fill: parent

            opacity: 0

            onStatusChanged: {
                if (status === Image.Ready)
                    anim.start();
            }

            Anim on opacity {
                id: anim

                type: Anim.SlowEffects
                running: false
                from: 0
                to: 1
            }

            Loader {
                anchors.fill: parent
                // The first frame shows until the backend is ready
                active: Images.isVideo(img.path) && Wallpapers.video.ready

                sourceComponent: VideoOutput {
                    id: videoOut

                    readonly property bool fed: img.path === root.source && root.playing

                    function update(): void {
                        if (fed)
                            Wallpapers.video.addOutput(videoOut);
                        else
                            Wallpapers.video.removeOutput(videoOut);
                    }

                    fillMode: VideoOutput.PreserveAspectCrop

                    onFedChanged: update()
                    Component.onCompleted: update()
                    Component.onDestruction: Wallpapers.video.removeOutput(videoOut)
                }
            }

            Timer {
                running: root.current !== img && root.current?.status === Image.Ready
                interval: anim.duration
                onTriggered: {
                    root.images = root.images.filter(image => image !== img);
                    img.destroy();
                }
            }
        }
    }
}
