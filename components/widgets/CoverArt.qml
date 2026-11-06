pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import M3Shapes
import Caelestia.Config
import qs.components
import qs.components.controls
import qs.components.images
import qs.services

Item {
    id: root

    readonly property alias shape: shape

    property color fallbackColour: Colours.layer(Colours.palette.m3surfaceContainerHighest, 2)

    Behavior on fallbackColour {
        CAnim {}
    }

    // The shape spins as a transform of its layer, and the art masks against that layer rotated in a shader, so
    // spinning re-renders neither the shape, its glow nor the art
    MaterialShape {
        id: shape

        implicitSize: root.width
        shape: MaterialShape.Cookie12Sided
        color: Qt.alpha(root.fallbackColour, 1)
        opacity: root.fallbackColour.a

        layer.enabled: true
        layer.smooth: true
        // Slight glow to separate from bg
        layer.effect: MultiEffect {
            shadowEnabled: true
            blurMax: 1
            shadowColor: Colours.palette.m3outline
            shadowOpacity: 0.3
        }

        Anim on rotation {
            running: true
            paused: !Players.active?.isPlaying
            from: 360
            to: 0
            duration: 23500
            easing.type: Easing.Linear
            loops: Animation.Infinite
        }
    }

    MaterialIcon {
        anchors.centerIn: parent

        grade: 200
        text: image.status === Image.Error ? "broken_image" : "art_track"
        color: Colours.palette.m3onSurfaceVariant
        fontStyle: Tokens.font.icon.size((parent.width * 0.35) || 1).build()
        opacity: image.status === Image.Null || image.status === Image.Error ? 1 : 0
        animate: true

        Behavior on opacity {
            Anim {
                type: Anim.DefaultEffects
            }
        }
    }

    Loader {
        anchors.centerIn: parent
        asynchronous: true
        active: opacity > 0
        opacity: image.status === Image.Loading ? 1 : 0

        sourceComponent: LoadingIndicator {
            implicitSize: root.width * 0.3
            color: Colours.palette.m3primaryContainer
        }

        Behavior on opacity {
            Anim {
                type: Anim.DefaultEffects
            }
        }
    }

    FadeImage {
        id: image

        anchors.fill: parent

        source: Players.getArtUrl(Players.active)

        layer.enabled: true
        layer.effect: ShaderEffect {
            readonly property Item maskSource: shape
            readonly property real angle: shape.rotation * Math.PI / 180

            fragmentShader: "qrc:/shaders/covermask.frag.qsb"
        }
    }
}
