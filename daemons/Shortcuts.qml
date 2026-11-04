pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia
import Caelestia.Config
import qs.components.misc
import qs.services
import qs.modules.nexus

Scope {
    id: root

    readonly property bool hasFullscreen: Hypr.focusedWorkspace?.toplevels.values.some(t => t.lastIpcObject.fullscreen > 1) ?? false

    // qmllint disable unresolved-type
    CustomShortcut {
        // qmllint enable unresolved-type
        name: "nexus"
        description: "Open nexus"
        onPressed: WindowFactory.create()
    }

    // qmllint disable unresolved-type
    CustomShortcut {
        // qmllint enable unresolved-type
        name: "showall"
        description: "Toggle launcher, dashboard and osd"
        onPressed: {
            if (root.hasFullscreen)
                return;
            const v = ShellState.forActive();
            v.launcher = v.dashboard = v.osd = v.utilities = !(v.launcher || v.dashboard || v.osd || v.utilities);
        }
    }

    // qmllint disable unresolved-type
    DrawerShortcut {
        name: "dashboard"
        description: "Toggle dashboard"
    }

    DrawerShortcut {
        name: "session"
        description: "Toggle session menu"
    }

    DrawerShortcut {
        name: "launcher"
        description: "Toggle launcher"
    }

    DrawerShortcut {
        name: "sidebar"
        description: "Toggle sidebar"
        available: GlobalConfig.sidebar.enabled
    }

    DrawerShortcut {
        name: "utilities"
        description: "Toggle utilities"
    }
    // qmllint enable unresolved-type

    IpcHandler {
        function toggle(drawer: string): void {
            if (list().split("\n").includes(drawer)) {
                if (root.hasFullscreen && ["launcher", "session", "dashboard"].includes(drawer))
                    return;
                const screenState = ShellState.forActive();
                screenState[drawer] = !screenState[drawer];
            } else {
                console.warn(lc, `Drawer "${drawer}" does not exist`);
            }
        }

        function list(): string {
            const screenState = ShellState.forActive();
            // focused and preload are monitor state, not drawers
            return Object.keys(screenState).filter(k => typeof screenState[k] === "boolean" && !["focused", "preload"].includes(k)).join("\n");
        }

        function isOpen(drawer: string): string {
            const screenState = ShellState.forActive();
            if (typeof screenState[drawer] !== "boolean")
                return "unknown";
            return screenState[drawer] ? "1" : "0";
        }

        target: "drawers"
    }

    IpcHandler {
        function open(): void {
            WindowFactory.create();
        }

        target: "nexus"
    }

    IpcHandler {
        function info(title: string, message: string, icon: string): void {
            Toaster.toast(title, message, icon, Toast.Info);
        }

        function success(title: string, message: string, icon: string): void {
            Toaster.toast(title, message, icon, Toast.Success);
        }

        function warn(title: string, message: string, icon: string): void {
            Toaster.toast(title, message, icon, Toast.Warning);
        }

        function error(title: string, message: string, icon: string): void {
            Toaster.toast(title, message, icon, Toast.Error);
        }

        target: "toaster"
    }

    LoggingCategory {
        id: lc

        name: "caelestia.qml.shortcuts"
        defaultLogLevel: LoggingCategory.Info
    }

    // Toggles a drawer on the active screen, unless a fullscreen app has focus
    // qmllint disable unresolved-type
    component DrawerShortcut: CustomShortcut {
        // qmllint enable unresolved-type
        id: shortcut

        property bool available: true

        onPressed: {
            if (root.hasFullscreen || !shortcut.available)
                return;
            const screenState = ShellState.forActive();
            screenState[shortcut.name] = !screenState[shortcut.name];
        }
    }
}
