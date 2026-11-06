import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import Caelestia.State
import qs.services

Scope {
    id: root

    Connections {
        function onInhibitIdleChanged(): void {
            if (RuntimeState.inhibitIdle)
                RuntimeState.inhibitingIdleSince = new Date().getTime();
        }

        target: RuntimeState
    }

    IdleInhibitor {
        enabled: RuntimeState.inhibitIdle
        // Reuse a window: a dedicated one costs a layer surface, render thread and GPU context even while disabled
        window: ShellState.componentsFor(Screens.screens[0])?.rootWindow ?? null
    }

    IpcHandler {
        function isEnabled(): bool {
            return RuntimeState.inhibitIdle;
        }

        function toggle(): void {
            RuntimeState.inhibitIdle = !RuntimeState.inhibitIdle;
        }

        function enable(): void {
            RuntimeState.inhibitIdle = true;
        }

        function disable(): void {
            RuntimeState.inhibitIdle = false;
        }

        target: "idleInhibitor"
    }
}
