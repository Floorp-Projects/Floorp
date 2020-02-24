/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

/**
 * Allows monitoring events targeted at the current account/device.
 */
interface AccountEventsObserver {
    fun onEvents(events: List<AccountEvent>)
}

typealias OuterDeviceCommandIncoming = DeviceCommandIncoming;

sealed class AccountEvent {
    // A tab with all its history entries (back button).
    class DeviceCommandIncoming(val command: OuterDeviceCommandIncoming) : AccountEvent()
    class ProfileUpdated : AccountEvent()
    class AccountAuthStateChanged : AccountEvent()
    class AccountDestroyed : AccountEvent()
    class DeviceConnected(val deviceName: String) : AccountEvent()
    class DeviceDisconnected(val deviceId: String, val isLocalDevice: Boolean) : AccountEvent()
}

/**
 * Incoming device commands (ie, targeted at the current device.)
 */
sealed class DeviceCommandIncoming {
    class TabReceived(val from: Device?, val entries: List<TabData>) : DeviceCommandIncoming()
}

/**
 * Outgoing device commands (ie, targeted at other devices.)
 */
sealed class DeviceCommandOutgoing {
    class SendTab(val title: String, val url: String) : DeviceCommandOutgoing()
}

data class TabData(
    val title: String,
    val url: String
)
