pragma ComponentBehavior: Bound

import qs.components
import qs.components.controls
import qs.components.effects
import qs.services
import qs.config
import qs.utils
import Quickshell
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    spacing: Appearance.spacing.small
    width: Config.bar.sizes.networkWidth

    StyledText {
        Layout.topMargin: Appearance.padding.normal
        Layout.rightMargin: Appearance.padding.small
        text: Notifs.notClosed.length > 0 ? qsTr("%1 notification%2").arg(Notifs.notClosed.length).arg(Notifs.notClosed.length === 1 ? "" : "s") : qsTr("Notifications")
        font.weight: 500
    }

    Loader {
        Layout.fillWidth: true
        Layout.rightMargin: Appearance.padding.small
        active: Notifs.notClosed.length === 0
        visible: active

        sourceComponent: StyledText {
            text: qsTr("No notifications")
            color: Colours.palette.m3onSurfaceVariant
            font.pointSize: Appearance.font.size.small
        }
    }

    Repeater {
        model: ScriptModel {
            values: Notifs.notClosed.slice(0, 5)
        }

        RowLayout {
            id: notifItem

            required property var modelData

            Layout.fillWidth: true
            Layout.rightMargin: Appearance.padding.small
            spacing: Appearance.spacing.small

            opacity: 0
            scale: 0.7

            Component.onCompleted: {
                opacity = 1;
                scale = 1;
            }

            Behavior on opacity {
                Anim {}
            }

            Behavior on scale {
                Anim {}
            }

            Loader {
                asynchronous: true

                sourceComponent: notifItem.modelData.appIcon.length > 0 ? iconComp : fallbackComp

                Component {
                    id: iconComp
                    ColouredIcon {
                        implicitWidth: Appearance.font.size.large * 1.5
                        implicitHeight: Appearance.font.size.large * 1.5
                        source: Quickshell.iconPath(notifItem.modelData.appIcon)
                        colour: Colours.palette.m3onSurface
                    }
                }

                Component {
                    id: fallbackComp
                    MaterialIcon {
                        text: "notifications"
                        font.pointSize: Appearance.font.size.large
                        color: Colours.palette.m3onSurfaceVariant
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Appearance.spacing.smaller / 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Appearance.spacing.small

                    StyledText {
                        Layout.fillWidth: true
                        text: notifItem.modelData.summary || notifItem.modelData.appName
                        elide: Text.ElideRight
                        font.weight: 500
                    }

                    StyledText {
                        text: notifItem.modelData.timeStr
                        color: Colours.palette.m3onSurfaceVariant
                        font.pointSize: Appearance.font.size.small
                    }
                }

                StyledText {
                    Layout.fillWidth: true
                    visible: notifItem.modelData.body.length > 0
                    text: notifItem.modelData.body
                    elide: Text.ElideRight
                    color: Colours.palette.m3onSurfaceVariant
                    font.pointSize: Appearance.font.size.small
                }
            }

            StyledRect {
                implicitWidth: implicitHeight
                implicitHeight: closeIcon.implicitHeight + Appearance.padding.small

                radius: Appearance.rounding.full
                color: "transparent"

                StateLayer {
                    color: Colours.palette.m3onSurface

                    function onClicked(): void {
                        notifItem.modelData.close();
                    }
                }

                MaterialIcon {
                    id: closeIcon

                    anchors.centerIn: parent
                    text: "close"
                    font.pointSize: Appearance.font.size.small
                    color: Colours.palette.m3onSurfaceVariant
                }
            }
        }
    }

    StyledRect {
        Layout.topMargin: Appearance.spacing.small
        Layout.fillWidth: true
        implicitHeight: clearBtn.implicitHeight + Appearance.padding.small * 2

        visible: Notifs.notClosed.length > 0
        radius: Appearance.rounding.full
        color: Colours.palette.m3primaryContainer

        StateLayer {
            color: Colours.palette.m3onPrimaryContainer

            function onClicked(): void {
                for (const notif of Notifs.list.slice())
                    notif.close();
            }
        }

        RowLayout {
            id: clearBtn

            anchors.centerIn: parent
            spacing: Appearance.spacing.small

            MaterialIcon {
                animate: true
                text: "clear_all"
                color: Colours.palette.m3onPrimaryContainer
            }

            StyledText {
                text: qsTr("Clear all")
                color: Colours.palette.m3onPrimaryContainer
            }
        }
    }
}
