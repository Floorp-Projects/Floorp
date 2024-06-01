/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.annotation.WorkerThread
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceCommandQueue
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.service.fxa.manager.FxaAccountManager

/**
 * Use cases for closing tabs that are open on other devices in the [DeviceConstellation].
 *
 * The use cases send commands to close tabs using the [FxaAccountManager].
 *
 * See [CloseTabsFeature] for the ability to close tabs on this device from
 * other devices.
 *
 * @param commands The queue used to send "close tab" commands to other devices.
 */
class CloseTabsUseCases(
    private val commands: DeviceCommandQueue<DeviceCommandQueue.Type.RemoteTabs>,
) {
    /**
     * Closes a tab that's currently open on another device.
     *
     * @param deviceId The ID of the device on which the tab is currently open.
     * @param url The URL of the tab to close.
     * @return Whether the command to close the tab was sent to the device.
     */
    @WorkerThread
    suspend fun close(deviceId: String, url: String) {
        commands.add(deviceId, DeviceCommandOutgoing.CloseTab(listOf(url)))
    }
}
