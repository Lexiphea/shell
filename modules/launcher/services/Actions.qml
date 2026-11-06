pragma Singleton

import ".."
import QtQuick
import Quickshell
import Caelestia.Config
import Caelestia.I18n
import Caelestia.Services
import qs.services
import qs.utils
import qs.modules.launcher.services

Singleton {
    id: root

    readonly property list<QtObject> list: variants.instances

    Variants {
        id: variants

        model: {
            const enableDangerous = GlobalConfig.launcher.enableDangerousActions;
            return GlobalConfig.launcher.actions.values.filter(a => a.enabled && (enableDangerous || !a.dangerous));
        }

        Action {}
    }

    component Action: QtObject {
        required property var modelData
        readonly property string name: modelData.name ? Tr.trMarked(modelData.name) : Tr.trCtx("Unnamed", "launcher action with no name")
        readonly property string desc: modelData.description ? Tr.trMarked(modelData.description) : Tr.trCtx("No description", "launcher action with no description")
        readonly property string icon: modelData.icon ?? "help_outline"
        readonly property list<string> command: modelData.command ?? []
        // Untranslated, so usage survives a language change
        readonly property string usageId: `action:${modelData.name}`

        function onClicked(list: AppList): void {
            if (command.length === 0)
                return;

            if (command[0] === "autocomplete" && command.length > 1) {
                list.search.text = `${command[1]} `;
                return;
            }

            list.screenState.launcher = false;
            Apps.registerUse(usageId);
            if (command[0] === "setMode" && command.length > 1)
                Colours.setMode(command[1]);
            else if (!SessionManager.exec(command))
                Exec.app(command);
        }
    }
}
