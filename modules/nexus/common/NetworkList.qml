pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Caelestia.Config
import Caelestia.I18n
import qs.components
import qs.components.controls
import qs.services
import qs.utils
import qs.modules.nexus

ItemList {
    id: root

    required property NexusState nState
    property int limit: 0 // 0 = show all
    property bool enableFilter

    property string passwordSsid // Network showing the inline password field
    property bool passwordConnecting

    signal networkSelected(ap: Nmcli.AccessPoint)
    signal passwordRejected

    function networkFilter(ap: Nmcli.AccessPoint): bool {
        return true;
    }

    function connectWithPassword(ap: Nmcli.AccessPoint, password: string): void {
        passwordConnecting = true;
        NetworkConnection.connectWithPassword(ap, password, result => {
            if (!root.passwordConnecting)
                return; // Nmcli can report the same result more than once
            root.passwordConnecting = false;
            if (result?.success) {
                if (root.passwordSsid === ap.ssid)
                    root.passwordSsid = "";
            } else {
                // Drop the profile saved with the wrong password so the next attempt asks again
                Nmcli.forgetNetwork(ap.ssid);
                root.passwordRejected();
            }
        });
    }

    showList: Nmcli.wifiEnabled
    placeholderIcon: Nmcli.wifiEnabled ? "wifi_find" : "signal_wifi_off"
    placeholderText: Nmcli.wifiEnabled ? Tr.tr("No networks found") : Tr.tr("Wi-Fi disabled")
    extraHeight: Nmcli.scanning ? Tokens.rounding.extraSmall : 0 // Inline so it isn't affected by anim
    list.anchors.top: scanningIndicator.bottom

    model: ScriptModel {
        values: {
            const connecting = Nmcli.connectingSsid();
            // Lower rank sorts higher in the list
            const rank = n => n.active ? 0 : n.ssid === connecting ? 1 : Nmcli.hasSavedProfile(n.ssid) ? 2 : 3;
            const sorted = [...Nmcli.networks].sort((a, b) => rank(a) - rank(b) || b.strength - a.strength);
            if (root.limit > 0 && sorted.length > root.limit)
                sorted.length = root.limit;
            return root.enableFilter ? sorted.filter(root.networkFilter) : sorted;
        }
    }

    delegate: Item {
        id: network

        required property int index
        required property var modelData
        property bool currentSelected
        readonly property bool askingPassword: root.passwordSsid === modelData.ssid
        readonly property bool disabled: currentSelected || Nmcli.connectingSsid() === modelData.ssid
        property real textOpacity: disabled ? 0.5 : 1

        // Unblock the row once it asks for a password, so clicking it again dismisses the field
        onAskingPasswordChanged: {
            if (askingPassword)
                currentSelected = false;
        }

        anchors.left: root.list.contentItem.left
        anchors.right: root.list.contentItem.right
        implicitHeight: header.implicitHeight + passwordArea.implicitHeight

        Behavior on textOpacity {
            Anim {
                type: Anim.DefaultEffects
            }
        }

        Connections {
            function onActiveChanged(): void {
                if (network.modelData.active)
                    network.currentSelected = false;
            }

            target: network.modelData
        }

        Connections {
            function onNetworkSelected(ap: Nmcli.AccessPoint): void {
                if (ap !== network.modelData)
                    network.currentSelected = false;
            }

            target: root
        }

        StateLayer {
            id: header

            anchors.left: parent.left
            anchors.right: parent.right
            implicitHeight: networkLayout.implicitHeight + networkLayout.anchors.margins * 2
            disabled: network.disabled
            radius: Tokens.rounding.extraSmall
            bottomLeftRadius: root?.last && network.index === root.list.count - 1 && !network.askingPassword ? Tokens.rounding.extraLarge : radius
            bottomRightRadius: root?.last && network.index === root.list.count - 1 && !network.askingPassword ? Tokens.rounding.extraLarge : radius
            anchors.fill: undefined

            onClicked: {
                if (network.askingPassword) {
                    root.passwordSsid = "";
                } else if (!network.modelData.active) {
                    root.passwordSsid = "";
                    NetworkConnection.handleConnect(network.modelData, ap => root.passwordSsid = ap.ssid);
                    network.currentSelected = true;
                    root.networkSelected(network.modelData);
                } else {
                    // Active network: open its detail/settings sub-page.
                    root.nState.selectedNetworkSsid = network.modelData.ssid;
                    root.nState.networkDetailsFromSaved = false;
                    root.nState.openSubPage(3);
                }
            }

            RowLayout {
                id: networkLayout

                anchors.fill: parent
                anchors.margins: Tokens.padding.large
                anchors.leftMargin: Tokens.padding.extraLarge
                anchors.rightMargin: Tokens.padding.extraLarge
                spacing: Tokens.spacing.medium

                MaterialIcon {
                    text: Icons.getNetworkIcon(network.modelData.strength)
                    color: network.modelData.active ? Colours.palette.m3primary : Colours.palette.m3onSurfaceVariant
                    fontStyle: Tokens.font.icon.medium
                    opacity: network.textOpacity
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    opacity: network.textOpacity

                    StyledText {
                        Layout.fillWidth: true
                        text: network.modelData.ssid
                        font: Tokens.font.body.small
                        elide: Text.ElideRight
                    }

                    StyledText {
                        Layout.fillWidth: true
                        text: {
                            const net = network.modelData;
                            let status = "";
                            if (net.active)
                                status = Tr.trCtx("Connected", "network connected");
                            else if (Nmcli.hasSavedProfile(net.ssid))
                                status = Tr.trCtx("Saved", "network saved");
                            if (status)
                                // TRANSLATORS: %1 = security type, %2 = connection status
                                return Tr.trCtx("Security: %1 • %2", "network security and status").arg(net.security).arg(status);
                            return Tr.trCtx("Security: %1", "network security").arg(net.security);
                        }
                        color: Colours.palette.m3outline
                        font: Tokens.font.label.small
                        elide: Text.ElideRight
                    }
                }

                AnimLoader {
                    sourceComp: Nmcli.connectingSsid() === network.modelData.ssid ? loadingComp : iconComp

                    Component {
                        id: iconComp

                        MaterialIcon {
                            text: network.modelData.active ? "settings" : "lock"
                            color: network.modelData.active ? Colours.palette.m3primary : Colours.palette.m3onSurfaceVariant
                            fontStyle: Tokens.font.icon.medium
                            opacity: network.textOpacity
                        }
                    }

                    Component {
                        id: loadingComp

                        LoadingIndicator {
                            implicitSize: Math.round(Tokens.font.icon.medium.pointSize * 1.3)
                        }
                    }
                }
            }
        }

        Item {
            id: passwordArea

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            implicitHeight: network.askingPassword ? passwordLoader.implicitHeight + passwordLoader.anchors.topMargin + Tokens.padding.large : 0
            clip: true

            Behavior on implicitHeight {
                Anim {
                    type: Anim.DefaultEffects
                }
            }

            Loader {
                id: passwordLoader

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: Tokens.padding.small // Room for the floating label
                anchors.leftMargin: Tokens.padding.extraLarge
                anchors.rightMargin: Tokens.padding.extraLarge
                active: opacity > 0
                opacity: network.askingPassword ? 1 : 0

                sourceComponent: RowLayout {
                    spacing: Tokens.spacing.small

                    StyledTextField {
                        id: passwordField

                        function submit(): void {
                            if (text && !root.passwordConnecting)
                                root.connectWithPassword(network.modelData, text);
                        }

                        Layout.fillWidth: true
                        enabled: !root.passwordConnecting
                        placeholderText: Tr.tr("Password")
                        leadingIcon: "key"
                        echoMode: TextInput.Password
                        errorText: Tr.tr("Connection failed — check the password")

                        Component.onCompleted: forceActiveFocus()
                        onAccepted: submit()
                        Keys.onEscapePressed: root.passwordSsid = ""

                        Connections {
                            function onPasswordRejected(): void {
                                passwordField.isError = true;
                                passwordField.forceActiveFocus();
                            }

                            target: root
                        }
                    }

                    TextButton {
                        // Match the field's outline, not its error text
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: passwordField.height - passwordField.supportingTextOffset
                        isRound: true
                        horizontalPadding: Tokens.padding.extraLarge
                        text: Tr.tr("Connect")
                        disabled: !passwordField.text || root.passwordConnecting
                        onClicked: passwordField.submit()
                    }
                }

                Behavior on opacity {
                    Anim {
                        type: Anim.DefaultEffects
                    }
                }
            }
        }
    }

    StyledProgressBar {
        id: scanningIndicator

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        implicitHeight: Nmcli.scanning ? Tokens.rounding.extraSmall : 0
        indeterminate: Nmcli.scanning

        Behavior on implicitHeight {
            Anim {
                type: Anim.DefaultEffects
            }
        }
    }
}
