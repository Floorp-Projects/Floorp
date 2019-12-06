/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.TabData
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.log.logger.Logger

/**
 * A feature that uses the [FxaAccountManager] to receive tabs.
 *
 * Adding push support with [AutoPushFeature] will allow for the account to be notified immediately.
 * If the push components are not used, the feature can still function
 * while tabs would only be received when refreshing the device state.
 *
 * See [SendTabUseCases] for the ability to send tabs to other devices.
 *
 * @param accountManager Firefox account manager.
 * @param owner Android lifecycle owner for the observers. Defaults to the [ProcessLifecycleOwner]
 * so that we can always observe events throughout the application lifecycle.
 * @param autoPause whether or not the observer should automatically be
 * paused/resumed with the bound lifecycle.
 * @param onTabsReceived the callback invoked with new tab(s) are received.
 */
class SendTabFeature(
    accountManager: FxaAccountManager,
    owner: LifecycleOwner = ProcessLifecycleOwner.get(),
    autoPause: Boolean = false,
    onTabsReceived: (Device?, List<TabData>) -> Unit
) {
    init {
        val deviceObserver = DeviceObserver(onTabsReceived)

        // Always observe the account for device events.
        accountManager.registerForDeviceEvents(deviceObserver, owner, autoPause)
    }
}

internal class DeviceObserver(
    private val onTabsReceived: (Device?, List<TabData>) -> Unit
) : DeviceEventsObserver {
    private val logger = Logger("DeviceObserver")

    override fun onEvents(events: List<DeviceEvent>) {
        events.asSequence()
            .filterIsInstance<DeviceEvent.TabReceived>()
            .forEach { event ->
                logger.debug("Showing ${event.entries.size} tab(s) received from deviceID=${event.from?.id}")

                onTabsReceived(event.from, event.entries)
            }
    }
}
