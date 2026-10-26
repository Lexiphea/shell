pragma Singleton

import Quickshell
import Quickshell.Io // Registers the type behind execDetached's object form

Singleton {
    // Set by systemd for processes it starts as a unit
    readonly property bool inSystemdUnit: !!Quickshell.env("INVOCATION_ID")

    // Launch an app in its own systemd scope. When the shell runs as a systemd service, a plain
    // execDetached child stays in the service's cgroup, so restarting the shell kills the app too
    function app(command: list<string>, workingDirectory: var): void {
        const cmd = inSystemdUnit ? ["systemd-run", "--user", "--scope", "--collect", "--quiet", "--expand-environment=no", "--", ...command] : command;
        Quickshell.execDetached(workingDirectory ? { command: cmd, workingDirectory } : cmd);
    }
}
