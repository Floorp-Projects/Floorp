/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCommandIncoming
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
    onTabsReceived: (Device?, List<TabData>) -> Unit,
) {
    init {
        val observer = EventsObserver(onTabsReceived)

        // Observe the account for all account events, although we'll ignore
        // non send-tab command events.
        accountManager.registerForAccountEvents(observer, owner, autoPause)
    }
}

internal class EventsObserver(
    private val onTabsReceived: (Device?, List<TabData>) -> Unit,
) : AccountEventsObserver {
    private val logger = Logger("EventsObserver")

    override fun onEvents(events: List<AccountEvent>) {
        events.asSequence()
            .filterIsInstance<AccountEvent.DeviceCommandIncoming>()
            .map { it.command }
            .filterIsInstance<DeviceCommandIncoming.TabReceived>()
            .forEach { command ->
                logger.debug("Showing ${command.entries.size} tab(s) received from deviceID=${command.from?.id}")

                onTabsReceived(command.from, command.entries)
            }
    }
}
