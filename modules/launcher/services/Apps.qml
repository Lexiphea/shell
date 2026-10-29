pragma Singleton

import QtQml
import Quickshell
import Caelestia.Config
import Caelestia.Models
import qs.utils

Singleton {
    id: root

    // Special prefix letter -> the entry field it searches instead of the default fields
    readonly property var prefixKeys: ({
            i: "id",
            c: "categories",
            d: "comment",
            e: "execString",
            w: "startupClass",
            g: "genericName",
            k: "keywords"
        })

    function launch(entry: DesktopEntry): void {
        appDb.registerLaunch(entry.id);

        if (entry.runInTerminal)
            Exec.app([...GlobalConfig.general.apps.terminal, `${Quickshell.shellDir}/assets/wrap_term_launch.sh`, ...entry.command], entry.workingDirectory);
        else
            Exec.app(entry.command, entry.workingDirectory);
    }

    // Commands are ranked in with the apps, except when a prefix narrows the search to an app field
    function search(search: string, commands: list<QtObject>): var {
        const prefix = GlobalConfig.launcher.specialPrefix;
        const special = search.startsWith(prefix) && search.charAt(prefix.length + 1) === " " ? search.charAt(prefix.length) : "";
        const key = prefixKeys[special] ?? "";
        const text = key || special === "t" ? search.slice(prefix.length + 2) : search;

        void appDb.apps; // Rerun bindings calling this when the apps or their order change
        const results = appDb.search(text, key, special ? [] : commands);
        return special === "t" ? results.filter(a => a.runInTerminal) : results;
    }

    AppDb {
        id: appDb

        path: `${Paths.state}/apps.sqlite`
        favouriteApps: GlobalConfig.launcher.favouriteApps
        entries: DesktopEntries.applications.values.filter(a => !Strings.testRegexList(GlobalConfig.launcher.hiddenApps, a.id))
    }
}
