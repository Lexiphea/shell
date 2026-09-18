pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia
import Caelestia.Config
import Caelestia.I18n

Singleton {
    id: root

    readonly property var selectedProviders: providerConfigs.filter(provider => provider.enabled).map(provider => provider.id)
    readonly property bool connected: providers.some(provider => provider.connected)
    readonly property bool connecting: providers.some(provider => provider.connecting)
    readonly property bool disconnecting: providers.some(provider => provider.disconnecting)
    readonly property var status: {
        const problem = providers.find(provider => provider.isSelected && (provider.status.state === "needs-auth" || provider.status.state === "error"));
        if (problem)
            return problem.status;
        const activeProvider = providers.find(provider => provider.connected || provider.connecting);
        return activeProvider ? activeProvider.status : disconnectedStatus();
    }

    readonly property var adapters: [wireguardAdapter, warpAdapter, netbirdAdapter, tailscaleAdapter]
    readonly property list<Provider> providers: []

    readonly property list<string> optionalKeys: ["displayName", "interface", "connectCmd", "disconnectCmd"]

    readonly property var providerConfigs: {
        const list = GlobalConfig.utilities.vpn.provider.values;
        const out = [];
        for (let i = 0; i < list.length; i++) {
            const value = list[i];
            out.push({
                index: i,
                id: value.id,
                name: value.name || "custom",
                displayName: value.displayName,
                interface: value.interface,
                connectCmd: value.connectCmd,
                disconnectCmd: value.disconnectCmd,
                enabled: value.enabled
            });
        }
        return out;
    }

    function disconnectedStatus(): var {
        return {
            connected: false,
            state: "disconnected",
            reason: "",
            authUrl: "",
            server: ""
        };
    }

    function syncProviders(): void {
        const configs = root.providerConfigs;
        const byId = new Map(configs.map(config => [config.id, config]));

        for (let i = root.providers.length - 1; i >= 0; i--) {
            if (!byId.has(root.providers[i].providerId)) {
                const removed = root.providers.splice(i, 1)[0];
                removed.destroy();
            }
        }

        const existing = new Map(root.providers.map(provider => [provider.providerId, provider]));
        for (const config of configs) {
            const provider = existing.get(config.id);
            if (provider)
                provider.lastIpcObject = config;
            else
                root.providers.push(providerComp.createObject(root, {
                    lastIpcObject: config
                }));
        }
    }

    function generateId(): string {
        return `vpn-${Date.now().toString(36)}-${Math.floor(Math.random() * 0x1000000).toString(36)}`;
    }

    function buildProviderProps(id: string, data: var): var {
        const props = {
            id: id,
            name: data.name,
            enabled: data.enabled === true
        };
        for (const key of root.optionalKeys)
            if (data[key].length > 0)
                props[key] = data[key];
        return props;
    }

    function providerIdAt(index: int): string {
        return GlobalConfig.utilities.vpn.provider.at(index)?.id ?? "";
    }

    function providerById(id: string): var {
        return root.providers.find(provider => provider.providerId === id);
    }

    function isSelected(id: string): bool {
        return root.selectedProviders.includes(id);
    }

    function setSelected(id: string, selected: bool): void {
        const entry = GlobalConfig.utilities.vpn.provider.values.find(value => value.id === id);
        if (entry)
            entry.enabled = selected;
    }

    function addProvider(data: var): void {
        GlobalConfig.utilities.vpn.provider.insert(root.buildProviderProps(root.generateId(), data));
    }

    function commitProviderUpdate(index: int, data: var): void {
        const entry = GlobalConfig.utilities.vpn.provider.at(index);
        if (!entry)
            return;
        if (!entry.id)
            entry.id = root.generateId();
        entry.name = data.name;
        for (const key of root.optionalKeys) {
            if (data[key].length > 0)
                entry[key] = data[key];
            else
                entry.resetOption(key);
        }
    }

    function updateProvider(index: int, data: var): void {
        const provider = root.providerById(root.providerIdAt(index));
        if (provider.connected || provider.connecting) {
            provider.pendingUpdate = data;
            provider.disconnect();
        } else {
            root.commitProviderUpdate(index, data);
        }
    }

    function commitProviderDelete(id: string): void {
        const list = GlobalConfig.utilities.vpn.provider;
        const index = list.values.findIndex(entry => entry.id === id);
        if (index >= 0)
            list.remove(index);
    }

    function deleteProvider(index: int): void {
        const id = root.providerIdAt(index);
        const provider = root.providerById(id);
        if (provider.connected || provider.connecting) {
            provider.deleteAfterDisconnect = true;
            provider.disconnect();
        } else {
            root.commitProviderDelete(id);
        }
    }

    function toggleProvider(index: int): void {
        const provider = root.providerById(root.providerIdAt(index));
        const select = !root.isSelected(provider.providerId);
        root.setSelected(provider.providerId, select);
        select ? provider.connect() : provider.disconnect();
    }

    function connect(): void {
        for (const provider of root.providers)
            if (provider.isSelected)
                provider.connect();
    }

    function disconnect(): void {
        for (const provider of root.providers)
            if (provider.connected || provider.connecting)
                provider.disconnect();
    }

    function toggle(): void {
        root.connected || root.connecting ? root.disconnect() : root.connect();
    }

    function checkStatus(): void {
        for (const provider of root.providers)
            if (provider.isSelected || provider.connecting || provider.disconnecting)
                provider.checkStatus();
    }

    function refreshStats(): void {
        for (const provider of root.providers)
            if (provider.isSelected && provider.connected)
                provider.refreshStats();
    }

    function parseTailscaleStatus(output: string): var {
        const status = disconnectedStatus();
        if (!output || output.trim().length === 0 || output.includes("Logged out") || output.includes("Stopped") || output.includes("not running"))
            return status;
        try {
            const data = JSON.parse(output);
            const backendState = data.BackendState || "";
            if (backendState === "Running") {
                status.connected = true;
                status.state = "connected";
                const peers = data.Peer || {};
                for (const key in peers) {
                    const peer = peers[key];
                    if (peer && peer.ExitNode) {
                        status.server = (peer.DNSName || peer.HostName || "").replace(/\.$/, "");
                        break;
                    }
                }
            } else if (backendState === "Starting") {
                status.state = "connecting";
            } else if (backendState === "NeedsLogin" || backendState === "NeedsMachineAuth") {
                status.state = "needs-auth";
                status.reason = backendState === "NeedsLogin" ? Tr.mark("Login required") : Tr.mark("Machine authorisation required");
                status.authUrl = data.AuthURL || "";
            }
        } catch (error) {
            if (output.includes("error") || output.includes("Error") || output.includes("failed"))
                status.reason = Tr.mark("Tailscale may not be running");
        }
        return status;
    }

    function parseNetBirdStatus(output: string): var {
        const status = disconnectedStatus();
        try {
            const data = JSON.parse(output);
            if (data.management?.connected && data.signal?.connected) {
                status.connected = true;
                status.state = "connected";
                const url = data.management?.url || data.management?.URL || "";
                if (url)
                    status.server = url.replace(/^https?:\/\//, "").replace(/:\d+$/, "");
            } else if (data.management?.error) {
                const error = data.management.error;
                status.state = error.includes("auth") || error.includes("login") ? "needs-auth" : "error";
                status.reason = error;
            }
        } catch (error) {
            status.state = "error";
            status.reason = Tr.mark("Failed to parse status");
        }
        return status;
    }

    function parseWarpStatus(output: string): var {
        const status = disconnectedStatus();
        if (output.includes("Registration Missing") || output.includes("registration") || output.includes("register") || output.includes("Unable to connect")) {
            status.state = "needs-auth";
            status.reason = Tr.mark("WARP registration required");
        } else if (output.includes("Disconnected")) {
            status.state = "disconnected";
        } else if (output.includes("Connecting")) {
            status.state = "connecting";
        } else if (output.includes("Connected")) {
            status.connected = true;
            status.state = "connected";
        } else {
            status.state = "error";
            status.reason = Tr.mark("Unknown WARP status");
        }
        return status;
    }

    function parseInterfaceStatus(output: string, iface: string): var {
        const status = disconnectedStatus();
        if (iface && output.includes(iface + ":")) {
            status.connected = true;
            status.state = "connected";
        }
        return status;
    }

    function parseWarpServer(output: string): string {
        for (const line of output.split("\n")) {
            const match = line.match(/Endpoint[^\d]*([\d.]+)/i);
            if (match)
                return match[1];
        }
        return "";
    }

    function extractAuthUrl(text: string): string {
        const match = text.match(/(https?:\/\/[^\s]+)/);
        return match ? match[1] : "";
    }

    function createAuthStatus(authUrl: string): var {
        return {
            connected: false,
            state: "needs-auth",
            reason: Tr.mark("Authentication required"),
            authUrl: authUrl,
            server: ""
        };
    }

    function migrateProviders(): void {
        const legacySelection = GlobalConfig.utilities.vpn.selectedProvider;
        for (const entry of GlobalConfig.utilities.vpn.provider.values) {
            if (!entry.id)
                entry.id = root.generateId();
            if (entry.id === legacySelection)
                entry.enabled = true;
        }
        if (legacySelection)
            GlobalConfig.utilities.vpn.selectedProvider = "";
    }

    function emitStatusToast(provider: var, statusObject: var): void {
        if (!GlobalConfig.utilities.toasts.vpnChanged)
            return;
        const name = provider.displayName || Tr.tr("VPN");
        switch (statusObject.state) {
        case "connected":
            Toaster.toast(Tr.tr("VPN connected"), Tr.tr("Connected to %1").arg(name), "vpn_key");
            break;
        case "disconnected":
            Toaster.toast(Tr.tr("VPN disconnected"), Tr.tr("Disconnected from %1").arg(name), "vpn_key_off");
            break;
        case "needs-auth":
            const authMsg = statusObject.reason ? Tr.trMarked(statusObject.reason) : Tr.tr("Authentication required");
            Toaster.toast(Tr.tr("VPN authentication required"), Tr.trCtx("%1: %2", "vpn name and reason").arg(name).arg(authMsg), "vpn_lock");
            break;
        case "error":
            const errMsg = statusObject.reason ? Tr.trMarked(statusObject.reason) : Tr.tr("Unknown error");
            Toaster.toast(Tr.tr("VPN error"), Tr.trCtx("%1: %2", "vpn name and reason").arg(name).arg(errMsg), "error");
            break;
        }
    }

    onProviderConfigsChanged: root.syncProviders()
    onConnectedChanged: GlobalConfig.utilities.vpn.enabled = connected

    Component.onCompleted: {
        root.migrateProviders();
        root.syncProviders();
    }

    Adapter {
        id: wireguardAdapter

        name: "wireguard"
        display: iface => iface
        connectCmd: iface => ["pkexec", "wg-quick", "up", iface]
        disconnectCmd: iface => ["pkexec", "wg-quick", "down", iface]
        // TRANSLATORS: %1 = a shell command, leave it untranslated
        connectHint: error => error.includes("Unknown device type") || error.includes("Protocol not supported") ? Tr.mark("WireGuard module not loaded. Run: %1", ["sudo modprobe wireguard"]) : ""
    }

    Adapter {
        id: warpAdapter

        name: "warp"
        display: "Warp"
        iface: "CloudflareWARP"
        service: "warp-svc"
        connectCmd: ["warp-cli", "connect"]
        disconnectCmd: ["warp-cli", "disconnect"]
        statusCmd: ["warp-cli", "status"]
        parse: output => root.parseWarpStatus(output)
        registerCmd: ["warp-cli", "registration", "new"]
        serverCmd: ["warp-cli", "tunnel", "stats"]
        parseServer: output => root.parseWarpServer(output)
    }

    Adapter {
        id: netbirdAdapter

        name: "netbird"
        display: "NetBird"
        iface: "wt0"
        service: "netbird"
        connectCmd: ["netbird", "up", "--no-browser"]
        disconnectCmd: ["netbird", "down"]
        statusCmd: ["netbird", "status", "--json"]
        parse: output => root.parseNetBirdStatus(output)
    }

    Adapter {
        id: tailscaleAdapter

        name: "tailscale"
        display: "Tailscale"
        iface: "tailscale0"
        service: "tailscaled"
        connectCmd: ["tailscale", "up"]
        disconnectCmd: ["tailscale", "down"]
        statusCmd: ["tailscale", "status", "--json"]
        parse: output => root.parseTailscaleStatus(output)
        // TRANSLATORS: %1 = a shell command, leave it untranslated
        connectHint: error => error.includes("Access denied") || error.includes("checkprefs access denied") ? Tr.mark("Permission denied. Run in terminal: %1", ["sudo tailscale set --operator=$USER"]) : ""
    }

    Connections {
        function onNetworkEvent(): void {
            root.checkStatus();
        }

        target: Nmcli // qmllint disable incompatible-type
    }

    Timer {
        running: root.selectedProviders.length > 0 || root.connecting || root.disconnecting
        repeat: true
        interval: root.connecting || root.disconnecting ? 5000 : 60000
        onTriggered: {
            root.checkStatus();
            root.refreshStats();
        }
    }

    Component {
        id: providerComp

        Provider {}
    }

    LoggingCategory {
        id: lc

        name: "caelestia.qml.services.vpn"
        defaultLogLevel: LoggingCategory.Info
    }

    component Provider: QtObject {
        id: provider

        required property var lastIpcObject
        readonly property int index: lastIpcObject.index
        readonly property string providerId: lastIpcObject.id
        readonly property string name: lastIpcObject.name
        readonly property string displayName: active.displayName
        readonly property string iface: active.interface
        readonly property var connectCmd: lastIpcObject.connectCmd
        readonly property var disconnectCmd: lastIpcObject.disconnectCmd
        readonly property bool isSelected: root.isSelected(providerId)

        property var status: root.disconnectedStatus()
        property bool connected: false
        property bool connectPending: false
        property bool disconnectPending: false
        readonly property bool connecting: connectProcess.running || connectPending
        readonly property bool disconnecting: disconnectProcess.running || disconnectPending
        property bool connectExited: true
        property bool disconnectExited: true
        property bool autoConnectPending: GlobalConfig.utilities.vpn.enabled && isSelected
        property var pendingUpdate: null
        property bool deleteAfterDisconnect: false
        property double connectedSince: 0
        property string bytesIn: ""
        property string bytesOut: ""
        property int pingMs: -1
        property string serverLocation: ""

        readonly property var active: {
            const adapter = root.adapters.find(value => value.name === provider.name) ?? null;
            const iface = provider.lastIpcObject.interface || (adapter ? adapter.iface : "") || (adapter ? "" : provider.name);
            const resolve = command => typeof command === "function" ? command(iface) : command;
            return {
                displayName: provider.lastIpcObject.displayName || resolve(adapter ? adapter.display : null) || provider.name,
                interface: iface,
                connectCmd: (provider.connectCmd.length > 0 ? provider.connectCmd : resolve(adapter ? adapter.connectCmd : null)) || [provider.name, "up"],
                disconnectCmd: (provider.disconnectCmd.length > 0 ? provider.disconnectCmd : resolve(adapter ? adapter.disconnectCmd : null)) || [provider.name, "down"],
                statusCmd: (adapter ? adapter.statusCmd : null) || ["ip", "link", "show"],
                parse: (adapter ? adapter.parse : null) || (output => root.parseInterfaceStatus(output, iface)),
                service: adapter ? adapter.service : `${provider.name}d`,
                connectHint: adapter ? adapter.connectHint : null,
                registerCmd: adapter ? adapter.registerCmd : null,
                serverCmd: adapter ? adapter.serverCmd : null,
                parseServer: adapter ? adapter.parseServer : null
            };
        }

        property Process statusProcess: Process {
            command: provider.active.statusCmd
            environment: ({LANG: "C.UTF-8", LC_ALL: "C.UTF-8"}) // qmllint disable incompatible-type
            stdout: StdioCollector {
                onStreamFinished: provider.updateStatus(provider.active.parse(text))
            }
            stderr: StdioCollector {
                onStreamFinished: {
                    if (!text.trim())
                        return;
                    const daemonDown = text.includes("doesn't appear to be running") || text.includes("failed to connect") || text.includes("daemon is not running") || text.includes("not running");
                    if (daemonDown && provider.active.service) {
                        const next = root.disconnectedStatus();
                        // TRANSLATORS: %1 = a shell command, leave it untranslated
                        next.reason = Tr.mark("Service not running (run: %1)", [`sudo systemctl start ${provider.active.service}`]);
                        provider.updateStatus(next);
                    }
                }
            }
        }

        property Process connectProcess: Process {
            onRunningChanged: {
                if (running) {
                    provider.connectExited = false;
                } else if (!provider.connectExited) {
                    provider.connectPending = false;
                    console.warn(lc, `Failed to start connect command '${command.join(" ")}'`);
                }
            }
            onExited: exitCode => { // qmllint disable signal-handler-parameters
                provider.connectExited = true;
                if (exitCode !== 0) {
                    provider.connectPending = false;
                    Toaster.toast(Tr.tr("VPN connection failed"), Tr.tr("Could not connect to %1").arg(provider.displayName), "vpn_key_alert");
                } else {
                    provider.checkTimer.start();
                }
            }
            stdout: SplitParser {
                onRead: data => {
                    const url = root.extractAuthUrl(data);
                    if (url)
                        provider.updateStatus(root.createAuthStatus(url));
                }
            }
            stderr: StdioCollector {
                onStreamFinished: {
                    const error = text.trim();
                    const hint = provider.active.connectHint ? provider.active.connectHint(error) : "";
                    const url = root.extractAuthUrl(error);
                    if (hint) {
                        const next = root.disconnectedStatus();
                        next.reason = hint;
                        provider.updateStatus(next);
                    } else if (url) {
                        provider.updateStatus(root.createAuthStatus(url));
                    } else if (error.includes("already exists")) {
                        provider.checkTimer.start();
                    }
                }
            }
        }

        property Process disconnectProcess: Process {
            onRunningChanged: {
                if (running) {
                    provider.disconnectExited = false;
                } else if (!provider.disconnectExited) {
                    provider.disconnectPending = false;
                    console.warn(lc, `Failed to start disconnect command '${command.join(" ")}'`);
                }
            }
            onExited: { // qmllint disable signal-handler-parameters
                provider.disconnectExited = true;
                provider.checkTimer.start();
            }
        }

        property Process registerProcess: Process {
            onExited: exitCode => { // qmllint disable signal-handler-parameters
                if (exitCode === 0)
                    provider.checkTimer.start();
            }
        }

        property Process statsProcess: Process {
            stdout: StdioCollector {
                onStreamFinished: {
                    const values = text.trim().split("\n").map(value => parseInt(value.trim(), 10)).filter(value => !isNaN(value));
                    if (values.length >= 2) {
                        provider.bytesIn = Units.formatBytes(values[0]);
                        provider.bytesOut = Units.formatBytes(values[1]);
                    }
                }
            }
        }

        property Process pingProcess: Process {
            stdout: StdioCollector {
                onStreamFinished: {
                    const match = text.match(/time[=<]\s*([\d.]+)\s*ms/i);
                    provider.pingMs = match ? Math.round(parseFloat(match[1])) : -1;
                }
            }
        }

        property Process serverProcess: Process {
            stdout: StdioCollector {
                onStreamFinished: {
                    if (provider.active.parseServer) {
                        const server = provider.active.parseServer(text);
                        if (server)
                            provider.serverLocation = server;
                    }
                }
            }
        }

        property Timer checkTimer: Timer {
            interval: 500
            onTriggered: provider.checkStatus()
        }

        function connect(): void {
            if (status.state === "needs-auth" && status.authUrl) {
                root.emitStatusToast(provider, status);
            } else if (!connected && !connecting) {
                connectPending = true;
                connectProcess.exec(active.connectCmd);
            }
        }

        function disconnect(): void {
            if ((connected || connecting) && !disconnecting) {
                disconnectPending = true;
                disconnectProcess.exec(active.disconnectCmd);
            }
        }

        function checkStatus(): void {
            if (!statusProcess.running)
                statusProcess.running = true;
        }

        function refreshStats(): void {
            if (!connected)
                return;
            if (iface.length > 0) {
                statsProcess.command = ["sh", "-c", `cat /sys/class/net/${iface}/statistics/rx_bytes /sys/class/net/${iface}/statistics/tx_bytes 2>/dev/null`];
                statsProcess.running = true;
                if (!pingProcess.running) {
                    pingProcess.command = ["sh", "-c", `ping -c1 -W2 -I ${iface} 1.1.1.1 2>/dev/null || ping -c1 -W2 1.1.1.1 2>/dev/null`];
                    pingProcess.running = true;
                }
            }
            if (active.serverCmd && serverLocation.length === 0)
                serverProcess.exec(active.serverCmd);
        }

        function updateStatus(newStatus: var): void {
            const oldState = status.state;
            if (newStatus.state === "needs-auth" && !newStatus.authUrl && status.authUrl)
                newStatus.authUrl = status.authUrl;
            status = newStatus;
            connected = newStatus.connected;
            connectPending = false;
            disconnectPending = false;
            if (newStatus.connected && newStatus.server)
                serverLocation = newStatus.server;
            if (oldState !== newStatus.state)
                root.emitStatusToast(provider, newStatus);
            if (autoConnectPending) {
                autoConnectPending = false;
                if (!newStatus.connected)
                    connect();
            }
            if (!newStatus.connected && (oldState !== "disconnected" || deleteAfterDisconnect || pendingUpdate))
                finishDisconnect();
            if (newStatus.state === "needs-auth" && active.registerCmd)
                registerProcess.exec(active.registerCmd);
        }

        function finishDisconnect(): void {
            if (deleteAfterDisconnect) {
                root.commitProviderDelete(providerId);
                return;
            }
            if (pendingUpdate) {
                const data = pendingUpdate;
                pendingUpdate = null;
                root.commitProviderUpdate(index, data);
                Qt.callLater(() => provider.connect());
            }
        }

        onConnectedChanged: {
            if (connected) {
                if (connectedSince === 0)
                    connectedSince = Date.now();
                if (isSelected)
                    refreshStats();
            } else {
                connectedSince = 0;
                bytesIn = "";
                bytesOut = "";
                serverLocation = "";
                pingMs = -1;
            }
        }

        Component.onCompleted: {
            if (isSelected)
                checkTimer.start();
        }
    }

    component Adapter: QtObject {
        required property string name
        property var display
        property string iface
        property string service
        property var connectCmd
        property var disconnectCmd
        property var statusCmd
        property var parse
        property var connectHint
        property var registerCmd
        property var serverCmd
        property var parseServer
    }
}
