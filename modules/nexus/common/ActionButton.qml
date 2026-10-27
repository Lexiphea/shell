import QtQuick
import QtQuick.Layouts
import Caelestia.Config
import qs.components
import qs.components.controls

// A ButtonRow button with an icon over a label
ButtonBase {
    id: root

    property string icon
    property string text

    implicitWidth: layout.implicitWidth + Tokens.padding.extraLarge * 2
    implicitHeight: layout.implicitHeight + Tokens.padding.medium * 2
    fillWidth: true
    shapeMorph: true
    isRound: true

    ColumnLayout {
        id: layout

        anchors.centerIn: parent
        spacing: 0

        MaterialIcon {
            Layout.alignment: Qt.AlignHCenter
            text: root.icon
            color: root.onColour
            fontStyle: Tokens.font.icon.medium
        }

        StyledText {
            Layout.alignment: Qt.AlignHCenter
            text: root.text
            color: root.onColour
        }
    }
}
