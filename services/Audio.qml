pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Services.Pipewire
import Caelestia
import Caelestia.Services
import qs.config

Singleton {
    id: root

    property string previousSinkName: ""
    property string previousSourceName: ""

    property list<PwNode> sinks: []
    property list<PwNode> sources: []
    property list<PwNode> streams: []

    readonly property PwNode sink: Pipewire.defaultAudioSink
    readonly property PwNode source: Pipewire.defaultAudioSource

    property bool outputMutedState: false
    property real outputVolumeState: 0
    property bool outputRefreshQueued: false

    readonly property bool muted: outputMutedState
    readonly property real volume: outputVolumeState
    readonly property real maxOutputVolume: 2.0
    readonly property int outputDisplayPercent: getVisibleOutputPercent()

    readonly property bool sourceMuted: !!source?.audio?.muted
    readonly property real sourceVolume: source?.audio?.volume ?? 0

    readonly property alias cava: cava
    readonly property alias beatTracker: beatTracker

    function getVisibleOutputVolume(): real {
        return maxOutputVolume > 0 ? Math.max(0, Math.min(1, volume / maxOutputVolume)) : 0;
    }

    function getVisibleOutputPercent(): int {
        return Math.max(0, Math.min(100, Math.round(getVisibleOutputVolume() * 100)));
    }

    function clampVisibleOutputVolume(value: real): real {
        return Math.max(0, Math.min(1, value));
    }

    function setVisibleOutputVolume(newVolume: real): void {
        setVolume(clampVisibleOutputVolume(newVolume) * maxOutputVolume);
    }

    function adjustVisibleOutputVolume(amount: real): void {
        setVisibleOutputVolume(getVisibleOutputVolume() + amount);
    }

    function setVolumeFromString(value: string): string {
        let targetVolume;

        if (value.endsWith("%-")) {
            const percent = parseFloat(value.slice(0, -2));
            targetVolume = getVisibleOutputVolume() - (percent / 100);
        } else if (value.startsWith("+") && value.endsWith("%")) {
            const percent = parseFloat(value.slice(1, -1));
            targetVolume = getVisibleOutputVolume() + (percent / 100);
        } else if (value.endsWith("%")) {
            const percent = parseFloat(value.slice(0, -1));
            targetVolume = percent / 100;
        } else if (value.startsWith("+")) {
            const increment = parseFloat(value.slice(1));
            targetVolume = getVisibleOutputVolume() + increment;
        } else if (value.endsWith("-")) {
            const decrement = parseFloat(value.slice(0, -1));
            targetVolume = getVisibleOutputVolume() - decrement;
        } else if (value.includes("%") || value.includes("-") || value.includes("+")) {
            return `Invalid audio format: ${value}\nExpected: 0.5, +0.1, 0.1-, 50%, +5%, 5%-`;
        } else {
            targetVolume = parseFloat(value);
        }

        if (isNaN(targetVolume))
            return `Failed to parse value: ${value}\nExpected: 0.5, +0.1, 0.1-, 50%, +5%, 5%-`;

        const clampedTargetVolume = clampVisibleOutputVolume(targetVolume);
        setVisibleOutputVolume(clampedTargetVolume);
        return `Set output volume to ${Math.round(clampedTargetVolume * 100)}%`;
    }

    function scheduleOutputRefresh(): void {
        outputRefreshDebounce.restart();
    }

    function refreshOutputState(): void {
        if (!outputStateProc.running)
            outputStateProc.running = true;
        else
            outputRefreshQueued = true;
    }

    function parseOutputState(output: string): void {
        const volumeMatch = output.match(/(\d+)%/);
        const muteMatch = output.match(/Mute:\s+(yes|no)/);

        if (volumeMatch) {
            const parsedVolume = parseInt(volumeMatch[1], 10) / 100;
            outputVolumeState = Math.max(0, Math.min(maxOutputVolume, parsedVolume));
        }

        if (muteMatch)
            outputMutedState = muteMatch[1] === "yes";
    }

    function setMuted(muted: bool): void {
        outputMutedState = muted;
        Quickshell.execDetached(["pactl", "set-sink-mute", "@DEFAULT_SINK@", muted ? "1" : "0"]);
        scheduleOutputRefresh();
    }

    function setVolume(newVolume: real): void {
        const clampedVolume = Math.max(0, Math.min(maxOutputVolume, newVolume));
        outputMutedState = false;
        outputVolumeState = clampedVolume;
        Quickshell.execDetached(["pactl", "set-sink-mute", "@DEFAULT_SINK@", "0"]);
        Quickshell.execDetached(["pactl", "set-sink-volume", "@DEFAULT_SINK@", `${Math.round(clampedVolume * 100)}%`]);
        scheduleOutputRefresh();
    }

    function incrementVolume(amount: real): void {
        setVolume(volume + (amount || Config.services.audioIncrement));
    }

    function decrementVolume(amount: real): void {
        setVolume(volume - (amount || Config.services.audioIncrement));
    }

    function setSourceVolume(newVolume: real): void {
        if (source?.ready && source?.audio) {
            source.audio.muted = false;
            source.audio.volume = Math.max(0, Math.min(Config.services.maxVolume, newVolume));
        }
    }

    function incrementSourceVolume(amount: real): void {
        setSourceVolume(sourceVolume + (amount || Config.services.audioIncrement));
    }

    function decrementSourceVolume(amount: real): void {
        setSourceVolume(sourceVolume - (amount || Config.services.audioIncrement));
    }

    function setAudioSink(newSink: PwNode): void {
        Pipewire.preferredDefaultAudioSink = newSink;
    }

    function setAudioSource(newSource: PwNode): void {
        Pipewire.preferredDefaultAudioSource = newSource;
    }

    function setStreamVolume(stream: PwNode, newVolume: real): void {
        if (stream?.ready && stream?.audio) {
            stream.audio.muted = false;
            stream.audio.volume = Math.max(0, Math.min(Config.services.maxVolume, newVolume));
        }
    }

    function setStreamMuted(stream: PwNode, muted: bool): void {
        if (stream?.ready && stream?.audio) {
            stream.audio.muted = muted;
        }
    }

    function getStreamVolume(stream: PwNode): real {
        return stream?.audio?.volume ?? 0;
    }

    function getStreamMuted(stream: PwNode): bool {
        return !!stream?.audio?.muted;
    }

    function getStreamName(stream: PwNode): string {
        if (!stream)
            return qsTr("Unknown");
        // Try application name first, then description, then name
        return stream.properties["application.name"] || stream.description || stream.name || qsTr("Unknown Application");
    }

    onSinkChanged: {
        if (!sink?.ready)
            return;

        const newSinkName = sink.description || sink.name || qsTr("Unknown Device");

        if (previousSinkName && previousSinkName !== newSinkName && Config.utilities.toasts.audioOutputChanged)
            Toaster.toast(qsTr("Audio output changed"), qsTr("Now using: %1").arg(newSinkName), "volume_up");

        previousSinkName = newSinkName;
        scheduleOutputRefresh();
    }

    onSourceChanged: {
        if (!source?.ready)
            return;

        const newSourceName = source.description || source.name || qsTr("Unknown Device");

        if (previousSourceName && previousSourceName !== newSourceName && Config.utilities.toasts.audioInputChanged)
            Toaster.toast(qsTr("Audio input changed"), qsTr("Now using: %1").arg(newSourceName), "mic");

        previousSourceName = newSourceName;
    }

    Component.onCompleted: {
        previousSinkName = sink?.description || sink?.name || qsTr("Unknown Device");
        previousSourceName = source?.description || source?.name || qsTr("Unknown Device");
        scheduleOutputRefresh();
    }

    Connections {
        function onValuesChanged(): void {
            const newSinks = [];
            const newSources = [];
            const newStreams = [];

            for (const node of Pipewire.nodes.values) {
                if (!node.isStream) {
                    if (node.isSink)
                        newSinks.push(node);
                    else if (node.audio)
                        newSources.push(node);
                } else if (node.audio) {
                    newStreams.push(node);
                }
            }

            root.sinks = newSinks;
            root.sources = newSources;
            root.streams = newStreams;
        }

        target: Pipewire.nodes
    }

    PwObjectTracker {
        objects: [...root.sinks, ...root.sources, ...root.streams]
    }

    Timer {
        id: outputRefreshDebounce

        interval: 100
        onTriggered: root.refreshOutputState()
    }

    Timer {
        id: outputSubscribeRestartTimer

        interval: 2000
        onTriggered: outputSubscribeProc.running = true
    }

    Process {
        id: outputStateProc

        command: ["sh", "-c", "pactl get-sink-volume @DEFAULT_SINK@; pactl get-sink-mute @DEFAULT_SINK@"]
        environment: ({
                LANG: "C.UTF-8",
                LC_ALL: "C.UTF-8"
            })
        stdout: StdioCollector {
            onStreamFinished: root.parseOutputState(text)
        }
        onExited: {
            if (root.outputRefreshQueued) {
                root.outputRefreshQueued = false;
                root.scheduleOutputRefresh();
            }
        }
    }

    Process {
        id: outputSubscribeProc

        running: true
        command: ["pactl", "subscribe"]
        environment: ({
                LANG: "C.UTF-8",
                LC_ALL: "C.UTF-8"
            })
        stdout: SplitParser {
            onRead: line => {
                if (line.includes("on sink") || line.includes("on server"))
                    root.scheduleOutputRefresh();
            }
        }
        onExited: outputSubscribeRestartTimer.start()
    }

    IpcHandler {
        target: "audio"

        function get(): real {
            return root.getVisibleOutputVolume();
        }

        function set(value: string): string {
            return root.setVolumeFromString(value);
        }

        function isMuted(): bool {
            return root.muted;
        }

        function mute(): void {
            root.setMuted(true);
        }

        function unmute(): void {
            root.setMuted(false);
        }

        function toggleMute(): void {
            root.setMuted(!root.muted);
        }
    }

    CavaProvider {
        id: cava

        bars: Config.services.visualiserBars
    }

    BeatTracker {
        id: beatTracker
    }
}
