/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.service.fxa.manager.FxaAccountManager

/**
 * A feature for closing tabs on this device from other devices
 * in the [DeviceConstellation].
 *
 * This feature receives commands to close tabs using the [FxaAccountManager].
 *
 * See [CloseTabsUseCases] for the ability to close tabs that are open on
 * other devices from this device.
 *
 * @param browserStore The [BrowserStore] that holds the currently open tabs.
 * @param accountManager The account manager.
 * @param owner The Android lifecycle owner for the observers. Defaults to
 * the [ProcessLifecycleOwner].
 * @param autoPause Whether or not the observer should automatically be
 * paused/resumed with the bound lifecycle.
 * @param onTabsClosed The callback invoked when one or more tabs are closed.
 */
class CloseTabsFeature(
    private val browserStore: BrowserStore,
    private val accountManager: FxaAccountManager,
    private val owner: LifecycleOwner = ProcessLifecycleOwner.get(),
    private val autoPause: Boolean = false,
    onTabsClosed: (Device?, List<String>) -> Unit,
) {
    @VisibleForTesting internal val observer = TabsClosedEventsObserver { device, urls ->
        val tabsToRemove = getTabsToRemove(urls)
        if (tabsToRemove.isNotEmpty()) {
            browserStore.dispatch(TabListAction.RemoveTabsAction(tabsToRemove.map { it.id }))
            onTabsClosed(device, tabsToRemove.map { it.content.url })
        }
    }

    /**
     * Begins observing the [accountManager] for "tabs closed" events.
     */
    fun observe() {
        accountManager.registerForAccountEvents(observer, owner, autoPause)
    }

    private fun getTabsToRemove(remotelyClosedUrls: List<String>): List<TabSessionState> {
        // The user might have the same URL open in multiple tabs on this device, and might want
        // to remotely close some or all of those tabs. Synced tabs don't carry enough
        // information to know which duplicates the user meant to close, so we use a heuristic:
        // if a URL appears N times in the remotely closed URLs list, we'll close up to
        // N instances of that URL.
        val countsByUrl = remotelyClosedUrls.groupingBy { it }.eachCount()
        return browserStore.state.tabs
            .groupBy { it.content.url }
            .asSequence()
            .mapNotNull { (url, tabs) ->
                countsByUrl[url]?.let { count -> tabs.take(count) }
            }
            .flatten()
            .toList()
    }
}

internal class TabsClosedEventsObserver(
    internal val onTabsClosed: (Device?, List<String>) -> Unit,
) : AccountEventsObserver {
    override fun onEvents(events: List<AccountEvent>) {
        // Group multiple commands from the same device, so that we can close
        // more tabs at once.
        events.asSequence()
            .filterIsInstance<AccountEvent.DeviceCommandIncoming>()
            .map { it.command }
            .filterIsInstance<DeviceCommandIncoming.TabsClosed>()
            .groupingBy { it.from }
            .fold(emptyList<String>()) { urls, command -> urls + command.urls }
            .forEach { (device, urls) ->
                onTabsClosed(device, urls)
            }
    }
}
