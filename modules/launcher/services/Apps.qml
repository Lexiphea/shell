pragma Singleton

import Quickshell
import Caelestia.Config
import Caelestia.Models
import qs.utils

Searcher {
    id: root

    // Special prefix letter -> the entry field it searches, weighted over the name
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
        appDb.incrementFrequency(entry.id);

        if (entry.runInTerminal)
            Exec.app([...GlobalConfig.general.apps.terminal, `${Quickshell.shellDir}/assets/wrap_term_launch.sh`, ...entry.command], entry.workingDirectory);
        else
            Exec.app(entry.command, entry.workingDirectory);
    }

    function search(search: string): var {
        const prefix = GlobalConfig.launcher.specialPrefix;
        const special = search.startsWith(prefix) && search.charAt(prefix.length + 1) === " " ? search.charAt(prefix.length) : "";
        const key = prefixKeys[special];

        keys = key ? [key, "name"] : ["name"];
        weights = key ? [0.9, 0.1] : [1];

        if (!key && special !== "t")
            return query(search).map(e => e.entry);

        const results = query(search.slice(prefix.length + 2)).map(e => e.entry);
        return special === "t" ? results.filter(a => a.runInTerminal) : results;
    }

    function selector(item: var): string {
        return keys.map(k => item[k]).join(" ");
    }

    list: appDb.apps
    useFuzzy: GlobalConfig.launcher.useFuzzy.apps

    AppDb {
        id: appDb

        path: `${Paths.state}/apps.sqlite`
        favouriteApps: GlobalConfig.launcher.favouriteApps
        entries: DesktopEntries.applications.values.filter(a => !Strings.testRegexList(GlobalConfig.launcher.hiddenApps, a.id))
    }
}
