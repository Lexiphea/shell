pragma Singleton

import QtQuick
import Quickshell
import Caelestia.I18n

Singleton {
    id: root

    property int refCount: 0

    readonly property string timeStr: format(Units.twelveHourClock ? "hh:mm:A" : "hh:mm")
    readonly property list<string> timeComponents: timeStr.split(":")
    readonly property string hourStr: timeComponents[0] ?? ""
    readonly property string minuteStr: timeComponents[1] ?? ""
    readonly property string amPmStr: timeComponents[2] ?? ""

    function format(fmt: string): string {
        return Qt.formatDateTime(clock.date, fmt);
    }

    SystemClock {
        id: clock

        precision: root.refCount > 0 ? SystemClock.Seconds : SystemClock.Minutes
    }
}
