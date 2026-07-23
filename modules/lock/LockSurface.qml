pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import Quickshell.Wayland
import Caelestia.Config
import qs.components
import qs.services

WlSessionLockSurface {
    id: root

    required property WlSessionLock lock
    required property Pam pam

    readonly property alias unlocking: unlockAnim.running
    property bool oledBlackActive

    contentItem.Config.screen: screen.name
    contentItem.Tokens.screen: screen.name

    color: "black"

    function stopOledBlackTimer(): void {
        oledBlackActive = false;
        blackoutDelay.stop();
    }

    function resetLockVisuals(): void {
        initAnim.stop();
        unlockAnim.stop();

        background.opacity = 0;
        lockContent.opacity = 1;
        lockContent.implicitWidth = lockContent.size;
        lockContent.implicitHeight = lockContent.size;
        lockContent.rotation = 180;
        lockContent.scale = 0;
        lockBg.radius = lockContent.radius;
        content.opacity = 0;
        content.scale = 0;
        lockIcon.opacity = 1;
        lockIcon.rotation = 180;

        initAnim.start();
    }

    function activateLockSurface(): void {
        resetLockVisuals();
        showLockUi();
    }

    function showLockUi(): void {
        if (oledBlackActive)
            oledBlackActive = false;
        restartOledBlackTimer();
    }

    function restartOledBlackTimer(): void {
        blackoutDelay.stop();
        if (root.lock.locked && root.visible && !root.oledBlackActive)
            blackoutDelay.start();
    }

    Connections {
        function onLockStateChanged(): void {
            if (root.lock.locked)
                root.activateLockSurface();
            else
                root.stopOledBlackTimer();
        }

        function onUnlock(): void {
            root.stopOledBlackTimer();
            unlockAnim.start();
        }

        target: root.lock
    }

    onVisibleChanged: {
        if (root.visible && root.lock.locked)
            activateLockSurface();
        else
            stopOledBlackTimer();
    }

    onUnlockingChanged: {
        if (unlocking)
            stopOledBlackTimer();
    }

    SequentialAnimation {
        id: blackoutDelay

        PauseAnimation {
            duration: 15000
        }
        PropertyAction {
            target: root
            property: "oledBlackActive"
            value: true
        }
    }

    SequentialAnimation {
        id: unlockAnim

        ParallelAnimation {
            Anim {
                target: lockContent
                properties: "implicitWidth,implicitHeight"
                to: lockContent.size
            }
            Anim {
                target: lockBg
                property: "radius"
                to: lockContent.radius
            }
            Anim {
                target: content
                property: "scale"
                to: 0
            }
            Anim {
                target: content
                property: "opacity"
                to: 0
                type: Anim.StandardSmall
            }
            Anim {
                target: lockIcon
                property: "opacity"
                to: 1
                type: Anim.StandardLarge
            }
            Anim {
                target: background
                property: "opacity"
                to: 0
                type: Anim.StandardLarge
            }
            SequentialAnimation {
                PauseAnimation {
                    duration: Tokens.anim.durations.small
                }
                Anim {
                    type: Anim.Standard
                    target: lockContent
                    property: "opacity"
                    to: 0
                }
            }
        }
        PropertyAction {
            target: root.lock
            property: "locked"
            value: false
        }
    }

    ParallelAnimation {
        id: initAnim

        Anim {
            target: background
            property: "opacity"
            to: 1
            type: Anim.StandardLarge
        }
        SequentialAnimation {
            ParallelAnimation {
                Anim {
                    target: lockContent
                    property: "scale"
                    to: 1
                    type: Anim.FastSpatial
                }
                Anim {
                    target: lockContent
                    property: "rotation"
                    to: 360
                    duration: Tokens.anim.durations.expressiveFastSpatial
                    easing: Tokens.anim.standardAccel
                }
            }
            ParallelAnimation {
                Anim {
                    target: lockIcon
                    property: "rotation"
                    to: 360
                    easing: Tokens.anim.standardDecel
                }
                Anim {
                    type: Anim.DefaultEffects
                    target: lockIcon
                    property: "opacity"
                    to: 0
                }
                Anim {
                    type: Anim.DefaultEffects
                    target: content
                    property: "opacity"
                    to: 1
                }
                Anim {
                    target: content
                    property: "scale"
                    to: 1
                }
                Anim {
                    target: lockBg
                    property: "radius"
                    to: lockContent.Tokens.rounding.extraLarge * 1.5
                }
                Anim {
                    target: lockContent
                    property: "implicitWidth"
                    to: (root.screen?.height ?? 0) * lockContent.Tokens.sizes.lock.heightMult * lockContent.Tokens.sizes.lock.ratio
                }
                Anim {
                    target: lockContent
                    property: "implicitHeight"
                    to: (root.screen?.height ?? 0) * lockContent.Tokens.sizes.lock.heightMult
                }
            }
        }
    }

    ScreencopyView {
        id: background

        anchors.fill: parent
        captureSource: root.screen
        opacity: 0
        visible: false

        layer.enabled: true
        layer.effect: MultiEffect {
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1
            blurMax: 64
            blurMultiplier: 1
        }
    }

    Item {
        id: lockContent

        readonly property int size: lockIcon.implicitHeight + Tokens.padding.large * 4
        readonly property int radius: size / 4 * Tokens.rounding.scale

        anchors.centerIn: parent
        implicitWidth: size
        implicitHeight: size

        visible: Config.lock.enabled && !root.oledBlackActive
        rotation: 180
        scale: 0

        StyledRect {
            id: lockBg

            anchors.fill: parent
            color: Colours.palette.m3surface
            radius: parent.radius
            opacity: Colours.transparency.enabled ? Colours.transparency.base : 1

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                blurMax: 15
                shadowColor: Qt.alpha(Colours.palette.m3shadow, 0.7)
            }
        }

        MaterialIcon {
            id: lockIcon

            anchors.centerIn: parent
            text: "lock"
            fontStyle: Tokens.font.icon.builders.extraLarge.scale(4).weight(Font.Bold).build()
            rotation: 180
        }

        Content {
            id: content

            anchors.centerIn: parent
            width: (root.screen?.height ?? 0) * Tokens.sizes.lock.heightMult * Tokens.sizes.lock.ratio - Tokens.padding.extraLargeIncreased
            height: (root.screen?.height ?? 0) * Tokens.sizes.lock.heightMult - Tokens.padding.extraLargeIncreased

            lock: root
            opacity: 0
            scale: 0
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
        visible: root.oledBlackActive
        z: 999
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.BlankCursor
        hoverEnabled: true
        visible: root.oledBlackActive
        z: 1000
        onClicked: root.showLockUi()
        onPressed: root.showLockUi()
        onWheel: root.showLockUi()
    }
}
