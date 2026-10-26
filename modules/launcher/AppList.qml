pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Caelestia
import Caelestia.Config
import qs.components
import qs.components.containers
import qs.components.controls
import qs.services
import qs.modules.launcher.items
import qs.modules.launcher.services

StyledListView {
    id: root

    required property SearchBar search
    required property ScreenState screenState

    property string displayText

    // Raycast style inline calculator. Like vicinae, it only shows when nothing else matches so it never takes
    // Enter from an app, unless the query starts with "=" to force it
    readonly property var calc: results.calc
    readonly property var results: {
        const values = Apps.search(displayText).concat(Actions.query(displayText));
        const query = displayText.trim();
        const forced = query.startsWith("=");
        if (!forced && (values.length > 0 || query.length < 3))
            return { values };

        const expr = forced ? query.slice(1) : query;
        const result = Soulver.eval(expr);
        if (!result || result.value === expr.trim())
            return { values };
        values.unshift(calcEntry);
        return { values, calc: Object.assign({ question: expr.trim() }, result) };
    }

    function syncDisplayText(): void {
        if (screenState.launcher)
            displayText = search.text;
    }

    model: ScriptModel {
        values: root.results.values
        onValuesChanged: root.currentIndex = 0
    }

    spacing: Tokens.spacing.small
    orientation: Qt.Vertical
    // The calculator card is double height
    implicitHeight: (Tokens.sizes.launcher.itemHeight + spacing) * Math.min(Config.launcher.maxShown, count) - spacing + (calc ? Tokens.sizes.launcher.itemHeight : 0)

    preferredHighlightBegin: 0
    preferredHighlightEnd: height
    highlightRangeMode: ListView.ApplyRange

    highlightFollowsCurrentItem: false
    highlight: StyledRect {
        radius: Tokens.rounding.large
        color: Colours.palette.m3onSurface
        opacity: 0.08

        y: root.currentItem?.y ?? 0
        implicitWidth: root.width
        implicitHeight: root.currentItem?.implicitHeight ?? 0

        Behavior on y {
            Anim {}
        }
    }

    Component.onCompleted: displayText = search.text

    delegate: appItem

    StyledScrollBar.vertical: StyledScrollBar {
        flickable: root
    }

    move: Transition {
        Anim {
            property: "y"
        }
        Anim {
            type: Anim.DefaultEffects
            property: "opacity"
            to: 1
        }
    }

    addDisplaced: Transition {
        Anim {
            property: "y"
            type: Anim.StandardSmall
        }
        Anim {
            type: Anim.DefaultEffects
            property: "opacity"
            to: 1
        }
    }

    displaced: Transition {
        Anim {
            property: "y"
        }
        Anim {
            type: Anim.DefaultEffects
            property: "opacity"
            to: 1
        }
    }

    QtObject {
        id: calcEntry
    }

    Component {
        id: appItem

        Loader {
            id: entry

            required property var modelData

            function activate(): void {
                item.activate();
            }

            anchors.left: parent?.left
            anchors.right: parent?.right

            sourceComponent: modelData === calcEntry ? soulverItem : (modelData as DesktopEntry) ? desktopEntry : actionEntry

            Component {
                id: desktopEntry

                AppItem {
                    modelData: entry.modelData as DesktopEntry
                    screenState: root.screenState
                }
            }

            Component {
                id: actionEntry

                ActionItem {
                    modelData: entry.modelData
                    list: root
                }
            }
        }
    }

    Component {
        id: soulverItem

        SoulverItem {
            list: root
        }
    }

    Connections {
        function onTextChanged() {
            root.syncDisplayText();
        }

        target: root.search
    }

    Connections {
        function onLauncherChanged() {
            root.syncDisplayText();
        }

        target: root.screenState
    }
}
