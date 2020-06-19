/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.withContext
import mozilla.appservices.remotetabs.RemoteTab
import mozilla.appservices.remotetabs.RemoteTabProviderException
import mozilla.components.concept.storage.Storage
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.appservices.remotetabs.RemoteTabsProvider
import mozilla.components.concept.sync.Device
import mozilla.appservices.remotetabs.SyncAuthInfo as RustSyncAuthInfo
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.support.utils.logElapsedTime

/**
 * An interface which defines read/write methods for remote tabs data.
 */
open class RemoteTabsStorage : Storage, SyncableStore {
    internal val api by lazy { RemoteTabsProvider() }
    private val scope by lazy { CoroutineScope(Dispatchers.IO) }
    internal val logger = Logger("RemoteTabsStorage")

    override suspend fun warmUp() {
        logElapsedTime(logger, "Warming up storage") { api }
    }

    /**
     * Store the locally opened tabs.
     * @param tabs The list of opened tabs, for all opened non-private windows, on this device.
     */
    suspend fun store(tabs: List<Tab>) {
        return withContext(scope.coroutineContext) {
            api.setLocalTabs(tabs.map {
                val activeTab = it.active()
                val urlHistory = listOf(activeTab.url) + it.previous().reversed().map { it.url }
                RemoteTab(activeTab.title, urlHistory, activeTab.iconUrl, it.lastUsed)
            })
        }
    }

    /**
     * Get all remote devices tabs.
     * @return A mapping of opened tabs per device.
     */
    suspend fun getAll(): Map<SyncClient, List<Tab>> {
        return withContext(scope.coroutineContext) {
            val allTabs = api.getAll() ?: return@withContext emptyMap<SyncClient, List<Tab>>()
            allTabs.map { device ->
                val tabs = device.tabs.map { tab ->
                    // Map RemoteTab to TabEntry
                    val title = tab.title
                    val icon = tab.icon
                    val lastUsed = tab.lastUsed ?: 0
                    val history = tab.urlHistory.reversed().map { url -> TabEntry(title, url, icon) }
                    Tab(history, tab.urlHistory.lastIndex, lastUsed)
                }
                // Map device to tabs
                SyncClient(device.clientId) to tabs
            }.toMap()
        }
    }

    /**
     * Syncs the remote tabs.
     *
     * @param authInfo The authentication information to sync with.
     * @param localId Local device ID from FxA.
     * @return Sync status of OK or Error
     */
    suspend fun sync(authInfo: SyncAuthInfo, localId: String): SyncStatus {
        return withContext(scope.coroutineContext) {
            try {
                logger.debug("Syncing...")
                api.sync(RustSyncAuthInfo(
                    authInfo.kid,
                    authInfo.fxaAccessToken,
                    authInfo.syncKey,
                    authInfo.tokenServerUrl
                ), localId)
                logger.debug("Successfully synced.")
                SyncStatus.Ok
            } catch (e: RemoteTabProviderException) {
                logger.error("Remote tabs exception while syncing", e)
                SyncStatus.Error(e)
            }
        }
    }

    override suspend fun runMaintenance() {
        // There's no such thing as maintenance for remote tabs, as it is a in-memory store.
    }

    override fun cleanup() {
        scope.coroutineContext.cancelChildren()
    }

    override fun getHandle(): Long {
        return api.getHandle()
    }
}

/**
 * Represents a Sync client that can be associated with a list of opened tabs.
 */
data class SyncClient(val id: String)

/**
 * A tab, which is defined by an history (think the previous/next button in your web browser) and
 * a currently active history entry.
 */
data class Tab(
    val history: List<TabEntry>,
    val active: Int,
    val lastUsed: Long
) {
    /**
     * The current active tab entry. In other words, this is the page that's currently shown for a
     * tab.
     */
    fun active(): TabEntry {
        return history[active]
    }

    /**
     * The list of tabs history entries that come before this tab. In other words, the "previous"
     * navigation button history list.
     */
    fun previous(): List<TabEntry> {
        return history.subList(0, active)
    }

    /**
     * The list of tabs history entries that come after this tab. In other words, the "next"
     * navigation button history list.
     */
    fun next(): List<TabEntry> {
        return history.subList(active + 1, history.lastIndex + 1)
    }
}

/**
 * A synced device and its list of tabs.
 */
data class SyncedDeviceTabs(
    val device: Device,
    val tabs: List<Tab>
)

/**
 * A Tab history entry.
 */
data class TabEntry(
    val title: String,
    val url: String,
    val iconUrl: String?
)
