pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Caelestia.Config
import Caelestia.I18n
import qs.components
import qs.services

Item {
    id: root

    required property var list

    function activate(): void {
        Quickshell.execDetached(["wl-copy", "--", list.calc.value]);
        list.screenState.launcher = false;
    }

    implicitHeight: Tokens.sizes.launcher.itemHeight * 2

    anchors.left: parent?.left
    anchors.right: parent?.right

    StyledRect {
        anchors.fill: parent
        radius: Tokens.rounding.large
        color: Colours.tPalette.m3surfaceContainer
    }

    StateLayer {
        radius: Tokens.rounding.large
        onClicked: root.activate()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Tokens.padding.large
        spacing: 0

        CalcPane {
            text: root.list.calc?.question ?? ""
            label: Tr.tr("Expression")
            textColour: Colours.palette.m3onSurfaceVariant
        }

        Item {
            Layout.fillHeight: true
            implicitWidth: arrow.implicitWidth + Tokens.padding.medium * 2

            StyledRect {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                implicitWidth: 1
                color: Colours.palette.m3outlineVariant
            }

            StyledRect {
                anchors.centerIn: parent
                implicitWidth: arrow.implicitWidth + Tokens.padding.small * 2
                implicitHeight: implicitWidth
                radius: Tokens.rounding.full
                color: Colours.tPalette.m3surfaceContainer
            }

            MaterialIcon {
                id: arrow

                anchors.centerIn: parent
                text: "arrow_forward"
                color: Colours.palette.m3outline
                fontStyle: Tokens.font.icon.medium
            }
        }

        CalcPane {
            text: root.list.calc?.value ?? ""
            label: root.list.calc?.type ?? ""
            textColour: Colours.palette.m3onSurface
            textFont: Tokens.font.title.large
        }
    }

    component CalcPane: ColumnLayout {
        id: pane

        required property string text
        required property string label
        required property color textColour
        property font textFont: Tokens.font.title.medium

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignVCenter
        spacing: Tokens.spacing.small

        StyledText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
            fontSizeMode: Text.HorizontalFit
            minimumPointSize: Tokens.font.body.medium.pointSize
            text: pane.text
            color: pane.textColour
            font: pane.textFont
        }

        StyledRect {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: chip.implicitWidth + Tokens.padding.medium * 2
            implicitHeight: chip.implicitHeight + Tokens.padding.small
            radius: Tokens.rounding.full
            color: Colours.palette.m3secondaryContainer

            StyledText {
                id: chip

                anchors.centerIn: parent
                text: pane.label
                color: Colours.palette.m3onSecondaryContainer
                font: Tokens.font.label.medium
            }
        }
    }
}
