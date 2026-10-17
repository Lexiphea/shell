pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io

Singleton {
    id: root

    readonly property alias running: props.running
    readonly property alias paused: props.paused
    readonly property alias elapsed: props.elapsed
    property int refCount: 0
    property bool needsStart
    property list<string> startCmd
    readonly property string windowRegion: "r=$({ hyprctl monitors -j; hyprctl clients -j; } | jq -rs '[.[0][] | .activeWorkspace.id, .specialWorkspace.id] as $ws | .[1][] | select(.mapped and (.hidden | not) and (.workspace.id | IN($ws[]))) | \"\\(.at[0]),\\(.at[1]) \\(.size[0])x\\(.size[1])\"' | slurp -r -f '%wx%h+%x+%y') && set -- -r \"$r\" \"$@\" && "
    property bool needsStop
    property bool needsPause

    function start(extraArgs = [], window = false): void {
        needsStart = true;
        startCmd = ["sh", "-c", (window ? windowRegion : "") + "exec caelestia record \"$@\" >/dev/null 2>&1", "sh", ...extraArgs];
        checkProc.running = true;
    }

    function stop(): void {
        needsStop = true;
        checkProc.running = true;
    }

    function togglePause(): void {
        needsPause = true;
        checkProc.running = true;
    }

    PersistentProperties {
        id: props

        property bool running: false
        property bool paused: false
        property real elapsed: 0 // Might get too large for int

        reloadableId: "recorder"
    }

    Process {
        id: checkProc

        running: true
        command: ["pidof", "gpu-screen-recorder"]
        onExited: code => { // qmllint disable signal-handler-parameters
            const running = code === 0;

            if (running && root.needsStop) {
                commandProc.exec(["caelestia", "record"]);
                props.running = false;
                props.paused = false;
            } else if (running && root.needsPause) {
                commandProc.exec(["caelestia", "record", "-p"]);
                props.paused = !props.paused;
            } else if (!running && root.needsStart) {
                commandProc.exec(root.startCmd);
                props.running = true;
                props.paused = false;
                props.elapsed = 0;
            } else if (running !== props.running && !commandProc.running) {
                // The recording was started/stopped outside the shell (e.g. via
                // keybind), or our command finished without reaching the optimistic state
                props.running = running;
                props.paused = false;
                props.elapsed = 0;
            }

            root.needsStart = false;
            root.needsStop = false;
            root.needsPause = false;
        }
    }

    Process {
        id: commandProc

        // The command owns the transition: `caelestia record` blocks on slurp for
        // region captures, and waits for the recorder to finalise the file when
        // stopping. Reconcile once it has actually finished.
        onExited: checkProc.running = true // qmllint disable signal-handler-parameters
    }

    // Only poll while something is showing the state, i.e. the utilities drawer is open
    Timer {
        interval: 1000
        running: root.refCount > 0
        repeat: true
        triggeredOnStart: true

        onTriggered: checkProc.running = true
    }

    Timer {
        interval: 1000
        running: props.running && !props.paused
        repeat: true

        onTriggered: props.elapsed++
    }
}
