/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.storage

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.FlowPreview
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.debounce
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.RemoteTabsStorage
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.Device
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * A storage that listens to the [BrowserStore] changes to synchronize the local tabs state
 * with [RemoteTabsStorage] and then synchronize with [accountManager].
 *
 * @param accountManager Account manager used to retrieve synced tabs.
 * @param store Browser store to observe for state changes.
 * @param tabsStorage Storage layer for tabs to sync.
 * @param debounceMillis Length to debounce rapid changes for storing and syncing.
 */
class SyncedTabsStorage(
    private val accountManager: FxaAccountManager,
    private val store: BrowserStore,
    private val tabsStorage: RemoteTabsStorage,
    private val debounceMillis: Long = 1000L,
) : SyncedTabsProvider {
    private var scope: CoroutineScope? = null

    /**
     * Start listening to browser store changes.
     */
    @OptIn(FlowPreview::class)
    fun start() {
        scope = store.flowScoped { flow ->
            flow.ifChanged { it.toSyncTabState() }
                .map { state ->
                    // TO-DO: https://github.com/mozilla-mobile/android-components/issues/5179
                    val iconUrl = null
                    state.tabs.filter { !it.content.private && !it.content.loading }.map { tab ->
                        // TO-DO: https://github.com/mozilla-mobile/android-components/issues/1340
                        val history = listOf(TabEntry(tab.content.title, tab.content.url, iconUrl))
                        Tab(history, 0, tab.lastAccess)
                    }
                }
                .debounce(debounceMillis)
                .collect { tabs ->
                    tabsStorage.store(tabs)
                    accountManager.syncNow(
                        reason = SyncReason.User,
                        customEngineSubset = listOf(SyncEngine.Tabs),
                        debounce = true,
                    )
                }
        }
    }

    /**
     * Stop listening to browser store changes.
     */
    fun stop() {
        scope?.cancel()
    }

    /**
     * See [SyncedTabsProvider.getSyncedDeviceTabs].
     */
    override suspend fun getSyncedDeviceTabs(): List<SyncedDeviceTabs> {
        val otherDevices = syncClients() ?: return emptyList()
        return tabsStorage.getAll()
            .mapNotNull { (client, tabs) ->
                val fxaDevice = otherDevices.find { it.id == client.id }

                fxaDevice?.let { SyncedDeviceTabs(fxaDevice, tabs.sortedByDescending { it.lastUsed }) }
            }
            .sortedByDescending {
                it.tabs.firstOrNull()?.lastUsed
            }
    }

    /**
     * List of synced devices.
     */
    @VisibleForTesting
    internal fun syncClients(): List<Device>? {
        accountManager.withConstellation {
            return state()?.otherDevices
        }
        return null
    }

    private data class SyncComponents(
        val selectedId: String?,
        val lastAccessed: List<Long>,
        val loadedTabs: List<TabSessionState>,
    )

    private fun BrowserState.toSyncTabState() = SyncComponents(
        selectedId = selectedTabId,
        lastAccessed = tabs.map { it.lastAccess },
        loadedTabs = tabs.filter { it.content.loading },
    )
}
