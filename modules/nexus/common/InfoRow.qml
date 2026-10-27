pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Caelestia.Config
import qs.components
import qs.services
import qs.modules.nexus.common

ConnectedRect {
    id: root

    property alias label: label.text
    property alias value: value.text
    property alias icon: icon.text

    Layout.fillWidth: true
    implicitHeight: rowLayout.implicitHeight + rowLayout.anchors.margins * 2

    RowLayout {
        id: rowLayout

        anchors.fill: parent
        anchors.margins: Tokens.padding.medium
        anchors.leftMargin: Tokens.padding.largeIncreased
        anchors.rightMargin: Tokens.padding.largeIncreased
        spacing: Tokens.spacing.medium

        MaterialIcon {
            id: icon

            visible: text
            color: Colours.palette.m3onSurfaceVariant
            fontStyle: Tokens.font.icon.small
        }

        StyledText {
            id: label

            Layout.fillWidth: true
            font: Tokens.font.body.small
            elide: Text.ElideRight
        }

        StyledText {
            id: value

            Layout.maximumWidth: root.width / 2
            horizontalAlignment: Text.AlignRight
            color: Colours.palette.m3onSurfaceVariant
            font: Tokens.font.body.small
            elide: Text.ElideRight
        }
    }
}
