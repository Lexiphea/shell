pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import qs.components
import qs.services

ColumnLayout {
    id: root

    required property color colour
    required property int parentSpacing

    property real gap: Hypr.capsLock && Hypr.numLock ? parentSpacing : 0

    spacing: Math.round(gap)

    Behavior on gap {
        Anim {
            type: Anim.SlowEffects
        }
    }

    LockIcon {
        locked: Hypr.capsLock
        icon: "keyboard_capslock_badge"
    }

    LockIcon {
        locked: Hypr.numLock
        icon: "looks_one"
    }

    component LockIcon: Item {
        id: lockIcon

        required property bool locked
        required property string icon
        property real animHeight: locked ? materialIcon.implicitHeight : 0

        implicitWidth: materialIcon.implicitWidth
        implicitHeight: Math.round(animHeight)

        Behavior on animHeight {
            Anim {
                type: Anim.SlowEffects
            }
        }

        MaterialIcon {
            id: materialIcon

            anchors.centerIn: parent

            scale: lockIcon.locked ? 1 : 0.5
            opacity: lockIcon.locked ? 1 : 0

            text: lockIcon.icon
            color: root.colour
            fill: 1
            grade: 25

            Behavior on opacity {
                Anim {
                    type: Anim.DefaultEffects
                }
            }

            Behavior on scale {
                Anim {}
            }
        }
    }
}
