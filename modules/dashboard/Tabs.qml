pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Caelestia.Config
import qs.components
import qs.components.controls
import qs.services

Item {
    id: root

    required property real nonAnimWidth
    required property ScreenState screenState
    required property var tabs

    readonly property alias count: repeater.count
    readonly property int currentIndex: screenState.dashboardTab
    readonly property Item currentItem: {
        // Trigger update when delegates are added or removed
        bar.children; // qmllint disable confusing-expression-statement
        return repeater.itemAt(currentIndex);
    }

    onCountChanged: {
        if (count > 0)
            screenState.dashboardTab = Math.min(screenState.dashboardTab, count - 1);
    }

    implicitHeight: bar.implicitHeight + bar.anchors.topMargin + indicator.implicitHeight + indicator.anchors.topMargin + separator.implicitHeight

    RowLayout {
        id: bar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: Tokens.sizes.dashboard.tabIndicatorSpacing

        spacing: 0

        Repeater {
            id: repeater

            model: ScriptModel {
                values: root.tabs
            }

            delegate: Tab {}
        }
    }

    Item {
        id: indicator

        readonly property real slotWidth: (root.nonAnimWidth - bar.spacing * (root.count - 1)) / root.count

        anchors.top: bar.bottom
        anchors.topMargin: 5

        implicitWidth: root.currentItem?.implicitWidth ?? slotWidth
        implicitHeight: 3

        x: slotWidth * root.currentIndex + (slotWidth - (root.currentItem?.implicitWidth ?? slotWidth)) / 2

        clip: true

        StyledRect {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            implicitHeight: parent.implicitHeight * 2

            color: Colours.palette.m3primary
            radius: Tokens.rounding.full
        }

        Behavior on x {
            Anim {}
        }

        Behavior on implicitWidth {
            Anim {}
        }
    }

    StyledRect {
        id: separator

        anchors.top: indicator.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        implicitHeight: 1
        color: Colours.palette.m3outlineVariant
    }

    component Tab: CustomMouseArea {
        id: tab

        required property var modelData
        required property int index
        readonly property bool current: index === root.currentIndex

        function onWheel(event: WheelEvent): void {
            if (event.angleDelta.y < 0)
                root.screenState.dashboardTab = Math.min(root.screenState.dashboardTab + 1, root.count - 1);
            else if (event.angleDelta.y > 0)
                root.screenState.dashboardTab = Math.max(root.screenState.dashboardTab - 1, 0);
        }

        Layout.fillWidth: true
        Layout.preferredWidth: 1 // Uniform width across all tabs
        implicitWidth: Math.max(icon.width, label.width)
        implicitHeight: icon.height + label.height

        StateLayer {
            anchors.fill: undefined
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            implicitHeight: parent.height + Tokens.sizes.dashboard.tabIndicatorSpacing * 2

            radius: Tokens.rounding.medium
            color: tab.current ? Colours.palette.m3primary : Colours.palette.m3onSurface
            onClicked: root.screenState.dashboardTab = tab.index
        }

        MaterialIcon {
            id: icon

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: label.top

            text: tab.modelData.iconName
            color: tab.current ? Colours.palette.m3primary : Colours.palette.m3onSurfaceVariant
            fill: tab.current ? 1 : 0
            fontStyle: Tokens.font.icon.medium

            Behavior on fill {
                Anim {
                    type: Anim.DefaultEffects
                }
            }
        }

        StyledText {
            id: label

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom

            text: tab.modelData.text
            color: tab.current ? Colours.palette.m3primary : Colours.palette.m3onSurfaceVariant
        }
    }
}
