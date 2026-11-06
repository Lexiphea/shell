pragma Singleton

import QtQuick
import qs.services

/**
 * NetworkConnection
 *
 * Centralized utility for network connection logic. Provides a single source of truth
 * for connecting to wireless networks, eliminating code duplication across
 * controlcenter components and bar popouts.
 *
 * Usage:
 * ```qml
 * import qs.utils
 *
 * // Provide a password dialog callback for secured networks without a saved profile
 * NetworkConnection.handleConnect(network, (network) => {
 *     // Show password dialog
 *     root.passwordNetwork = network;
 * });
 * ```
 */
QtObject {
    id: root

    /**
     * Handle network connection with automatic disconnection if needed.
     * If there's an active network different from the target, disconnects first,
     * then connects to the target network.
     *
     * @param network The network object to connect to (must have ssid property)
     * @param onPasswordNeeded Optional callback function(network) called when password is needed
     */
    function handleConnect(network, onPasswordNeeded): void {
        if (!network) {
            return;
        }

        if (Nmcli.active && Nmcli.active.ssid !== network.ssid) {
            Nmcli.disconnectFromNetwork();
            Qt.callLater(() => {
                root.connectToNetwork(network, onPasswordNeeded);
            });
        } else {
            root.connectToNetwork(network, onPasswordNeeded);
        }
    }

    /**
     * Connect to a wireless network.
     * Handles both secured and open networks, checks for saved profiles,
     * and shows password dialog if needed.
     *
     * @param network The network object to connect to (must have ssid, isSecure, bssid properties)
     * @param onPasswordNeeded Optional callback function(network) called when password is needed
     */
    function connectToNetwork(network, onPasswordNeeded): void {
        if (!network) {
            return;
        }

        if (network.isSecure && !Nmcli.hasSavedProfile(network.ssid)) {
            Nmcli.connectToNetwork(network.ssid, "", network.bssid, result => {
                if (result.needsPassword) {
                    Nmcli.clearPendingConnection();
                    onPasswordNeeded?.(network);
                }
            });
        } else {
            Nmcli.connectToNetwork(network.ssid, "", network.bssid, null);
        }
    }

    /**
     * Connect to a wireless network with a provided password.
     * Used by password dialogs when the user has already entered a password.
     *
     * @param network The network object to connect to (must have ssid, bssid properties)
     * @param password The password to use for connection
     * @param onResult Optional callback function(result) called with connection result
     */
    function connectWithPassword(network, password, onResult): void {
        if (!network) {
            return;
        }

        Nmcli.connectToNetwork(network.ssid, password || "", network.bssid || "", onResult || null);
    }
}
