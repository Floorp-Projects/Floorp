/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

/**
 * Allows monitoring events targeted at the current device.
 */
interface DeviceEventsObserver {
    fun onEvents(events: List<DeviceEvent>)
}

/**
 * Incoming device events, targeted at the current device.
 */
sealed class DeviceEvent {
    class TabReceived(val from: Device?, val entries: List<TabData>) : DeviceEvent()
}

/**
 * Outgoing device events, targeted at other devices.
 */
sealed class DeviceEventOutgoing {
    class SendTab(val title: String, val url: String) : DeviceEventOutgoing()
}

data class TabData(
    val title: String,
    val url: String
)
