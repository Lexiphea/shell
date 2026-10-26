pragma Singleton

import Quickshell
import Quickshell.Io
import Quickshell.Wayland as Wayland
import qs.services

Singleton {
    id: root

    property alias enabled: props.enabled
    readonly property alias enabledSince: props.enabledSince

    onEnabledChanged: {
        if (enabled)
            props.enabledSince = new Date();
    }

    PersistentProperties {
        id: props

        property bool enabled
        property date enabledSince

        reloadableId: "idleInhibitor"
    }

    Wayland.IdleInhibitor {
        enabled: props.enabled
        // Reuse a window: a dedicated one costs a layer surface, render thread and GPU context even while disabled
        window: ShellState.componentsFor(Screens.screens[0])?.rootWindow ?? null
    }

    IpcHandler {
        function isEnabled(): bool {
            return props.enabled;
        }

        function toggle(): void {
            props.enabled = !props.enabled;
        }

        function enable(): void {
            props.enabled = true;
        }

        function disable(): void {
            props.enabled = false;
        }

        target: "idleInhibitor"
    }
}
