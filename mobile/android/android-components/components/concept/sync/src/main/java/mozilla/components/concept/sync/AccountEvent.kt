/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

/**
 * Allows monitoring events targeted at the current account/device.
 */
interface AccountEventsObserver {
    /** The callback called when an account event is received */
    fun onEvents(events: List<AccountEvent>)
}

typealias OuterDeviceCommandIncoming = DeviceCommandIncoming

/**
 * Incoming account events.
 */
sealed class AccountEvent {
    /** An incoming command from another device */
    data class DeviceCommandIncoming(val command: OuterDeviceCommandIncoming) : AccountEvent()

    /** The account's profile was updated */
    object ProfileUpdated : AccountEvent()

    /** The authentication state of the account changed - eg, the password changed */
    object AccountAuthStateChanged : AccountEvent()

    /** The account itself was destroyed */
    object AccountDestroyed : AccountEvent()

    /** Another device connected to the account */
    data class DeviceConnected(val deviceName: String) : AccountEvent()

    /** A device (possibly this one) disconnected from the account */
    data class DeviceDisconnected(val deviceId: String, val isLocalDevice: Boolean) : AccountEvent()
}

/**
 * Incoming device commands (ie, targeted at the current device.)
 */
sealed class DeviceCommandIncoming {
    /** A command to open a list of tabs on the current device */
    class TabReceived(val from: Device?, val entries: List<TabData>) : DeviceCommandIncoming()
}

/**
 * Outgoing device commands (ie, targeted at other devices.)
 */
sealed class DeviceCommandOutgoing {
    /** A command to open a tab on another device */
    class SendTab(val title: String, val url: String) : DeviceCommandOutgoing()
}

/**
 * Information about a tab sent with tab related commands.
 */
data class TabData(
    val title: String,
    val url: String,
)
