import QtQml
import Quickshell
import qs.services

PersistentProperties {
    required property ShellScreen modelData

    readonly property bool focused: Hypr.monitorFor(modelData) === Hypr.focusedMonitor
    // Panels preload once the monitor has had focus and stay loaded, so moving between monitors doesn't rebuild them
    property bool preload

    // Drawer visibilities
    property bool bar
    property bool osd
    property bool session
    property bool launcher
    property bool dashboard
    property bool utilities
    property bool sidebar

    // Dashboard state
    property int dashboardTab
    property date dashboardDate: new Date()

    onFocusedChanged: {
        if (focused)
            preload = true;
    }
    Component.onCompleted: {
        if (focused)
            preload = true;
    }
}
