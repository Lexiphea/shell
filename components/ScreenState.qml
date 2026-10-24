import Quickshell
import qs.services

PersistentProperties {
    required property ShellScreen modelData

    // Whether this screen's monitor has focus. Panels stay preloaded only here
    readonly property bool focused: Hypr.monitorFor(modelData) === Hypr.focusedMonitor

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
}
