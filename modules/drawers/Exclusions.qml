pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Caelestia.Config
import qs.components.containers
import qs.modules.bar as Bar

Scope {
    id: root

    required property ShellScreen screen
    required property Bar.BarWrapper bar

    // An unpinned bar with no border reserves nothing, so skip the window until it does
    LazyLoader {
        active: root.bar.exclusiveZone > 0

        ExclusionZone {
            anchors.left: true
            exclusiveZone: root.bar.exclusiveZone
        }
    }

    // A zero-thickness border reserves nothing, so skip these windows (each costs a render thread and GPU context)
    LazyLoader {
        active: GlobalConfig.forScreen(root.screen.name).border.thickness > 0

        Scope {
            ExclusionZone {
                anchors.top: true
            }

            ExclusionZone {
                anchors.right: true
            }

            ExclusionZone {
                anchors.bottom: true
            }
        }
    }

    component ExclusionZone: StyledWindow {
        screen: root.screen
        name: "border-exclusion"
        exclusiveZone: contentItem.Config.border.thickness
        mask: Region {}
        implicitWidth: 1
        implicitHeight: 1
    }
}
