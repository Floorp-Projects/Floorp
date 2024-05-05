/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceCommandOutgoing
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
 * @param accountManager The account manager.
 */
class CloseTabsUseCases(private val accountManager: FxaAccountManager) {
    /**
     * Closes a tab that's currently open on another device.
     *
     * @param deviceId The ID of the device on which the tab is currently open.
     * @param url The URL of the tab to close.
     * @return Whether the command to close the tab was sent to the device.
     */
    @WorkerThread
    suspend fun close(deviceId: String, url: String): Boolean {
        filterCloseTabsDevices(accountManager) { constellation, devices ->
            val device = devices.firstOrNull { it.id == deviceId }
            device?.let {
                return constellation.sendCommandToDevice(
                    device.id,
                    DeviceCommandOutgoing.CloseTab(listOf(url)),
                )
            }
        }

        return false
    }
}

@VisibleForTesting
internal inline fun filterCloseTabsDevices(
    accountManager: FxaAccountManager,
    block: (DeviceConstellation, Collection<Device>) -> Unit,
) {
    val constellation = accountManager.authenticatedAccount()?.deviceConstellation() ?: return

    constellation.state()?.let { state ->
        state.otherDevices.filter {
            it.capabilities.contains(DeviceCapability.CLOSE_TABS)
        }.let { devices ->
            block(constellation, devices)
        }
    }
}
