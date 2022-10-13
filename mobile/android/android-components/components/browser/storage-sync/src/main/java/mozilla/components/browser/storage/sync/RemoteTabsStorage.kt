/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.withContext
import mozilla.appservices.remotetabs.RemoteTab
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.storage.Storage
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.logElapsedTime
import java.io.File
import mozilla.appservices.remotetabs.InternalException as RemoteTabProviderException
import mozilla.appservices.remotetabs.TabsStore as RemoteTabsProvider

private const val TABS_DB_NAME = "tabs.sqlite"

/**
 * An interface which defines read/write methods for remote tabs data.
 */
open class RemoteTabsStorage(
    private val context: Context,
    private val crashReporter: CrashReporting? = null,
) : Storage, SyncableStore {
    internal val api by lazy { RemoteTabsProvider(File(context.filesDir, TABS_DB_NAME).canonicalPath) }
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
            try {
                api.setLocalTabs(
                    tabs.map {
                        val activeTab = it.active()
                        val urlHistory = listOf(activeTab.url) + it.previous().reversed().map { it.url }
                        RemoteTab(activeTab.title, urlHistory, activeTab.iconUrl, it.lastUsed)
                    },
                )
            } catch (e: RemoteTabProviderException) {
                crashReporter?.submitCaughtException(e)
            }
        }
    }

    /**
     * Get all remote devices tabs.
     * @return A mapping of opened tabs per device.
     */
    suspend fun getAll(): Map<SyncClient, List<Tab>> {
        return withContext(scope.coroutineContext) {
            try {
                api.getAll().map { device ->
                    val tabs = device.remoteTabs.map { tab ->
                        // Map RemoteTab to TabEntry
                        val title = tab.title
                        val icon = tab.icon
                        val lastUsed = tab.lastUsed
                        val history = tab.urlHistory.reversed().map { url -> TabEntry(title, url, icon) }
                        Tab(history, tab.urlHistory.lastIndex, lastUsed)
                    }
                    // Map device to tabs
                    SyncClient(device.clientId) to tabs
                }.toMap()
            } catch (e: RemoteTabProviderException) {
                crashReporter?.submitCaughtException(e)
                return@withContext emptyMap()
            }
        }
    }

    override suspend fun runMaintenance() {
        // There's no such thing as maintenance for remote tabs, as it is a in-memory store.
    }

    override fun cleanup() {
        scope.coroutineContext.cancelChildren()
    }

    override fun registerWithSyncManager() {
        return api.registerWithSyncManager()
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
    val lastUsed: Long,
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
    val tabs: List<Tab>,
)

/**
 * A Tab history entry.
 */
data class TabEntry(
    val title: String,
    val url: String,
    val iconUrl: String?,
)
