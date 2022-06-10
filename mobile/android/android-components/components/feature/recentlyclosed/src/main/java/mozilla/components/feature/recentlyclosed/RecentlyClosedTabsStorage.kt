/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import mozilla.components.browser.session.storage.FileEngineSessionStateStorage
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSessionStateStorage
import mozilla.components.feature.recentlyclosed.db.RecentlyClosedTabsDatabase
import mozilla.components.feature.recentlyclosed.db.toRecentlyClosedTabEntity
import mozilla.components.support.base.log.logger.Logger

/**
 * Wraps exceptions that are caught by [RecentlyClosedTabsStorage].
 * Instances of this class are submitted via [CrashReporting]. This wrapping helps easily identify
 * exceptions related to [RecentlyClosedTabsStorage].
 */
private class RecentlyClosedTabsStorageException(e: Throwable) : Throwable(e)

/**
 * A storage implementation that saves snapshots of recently closed tabs / sessions.
 */
class RecentlyClosedTabsStorage(
    context: Context,
    engine: Engine,
    private val crashReporting: CrashReporting,
    private val engineStateStorage: EngineSessionStateStorage = FileEngineSessionStateStorage(context, engine)
) : RecentlyClosedMiddleware.Storage {
    private val logger = Logger("RecentlyClosedTabsStorage")

    @VisibleForTesting
    internal var database: Lazy<RecentlyClosedTabsDatabase> =
        lazy { RecentlyClosedTabsDatabase.get(context) }

    /**
     * Returns an observable list of [TabState]s.
     */
    @Suppress("TooGenericExceptionCaught")
    override suspend fun getTabs(): Flow<List<TabState>> {
        return database.value.recentlyClosedTabDao().getTabs()
            .catch { exception ->
                crashReporting.submitCaughtException(RecentlyClosedTabsStorageException(exception))
                // If the database is "corrupted" then we clean the database and also the file storage
                // to allow for a fresh set of recently closed tabs later.
                removeAllTabs()
                // Inform all observers of this data that recent tabs are cleared
                // to prevent users from trying to restore nonexistent recently closed tabs.
                emit(emptyList())
            }
            .map { list ->
                list.map { it.asTabState() }
            }
    }

    /**
     * Removes the given [TabState].
     */
    override suspend fun removeTab(recentlyClosedTab: TabState) {
        val entity = recentlyClosedTab.toRecentlyClosedTabEntity()
        engineStateStorage.delete(entity.uuid)
        database.value.recentlyClosedTabDao().deleteTab(entity)
    }

    /**
     * Removes all [TabState]s.
     */
    override suspend fun removeAllTabs() {
        engineStateStorage.deleteAll()
        database.value.recentlyClosedTabDao().removeAllTabs()
    }

    /**
     * Adds up to [maxTabs] [TabSessionState]s to storage, and then prunes storage to keep only the newest [maxTabs].
     */
    @Suppress("TooGenericExceptionCaught")
    override suspend fun addTabsToCollectionWithMax(
        tabs: List<RecoverableTab>,
        maxTabs: Int
    ) {
        try {
            tabs.takeLast(maxTabs).forEach { addTabState(it) }
            pruneTabsWithMax(maxTabs)
        } catch (e: Exception) {
            crashReporting.submitCaughtException(RecentlyClosedTabsStorageException(e))
        }
    }

    /**
     * @return An [EngineSessionStateStorage] instance used to persist engine state of tabs.
     */
    fun engineStateStorage(): EngineSessionStateStorage {
        return engineStateStorage
    }

    private suspend fun pruneTabsWithMax(maxTabs: Int) {
        val tabs = database.value.recentlyClosedTabDao().getTabs().first()

        // No pruning required
        if (tabs.size <= maxTabs) return

        tabs.subList(maxTabs, tabs.size).forEach { entity ->
            engineStateStorage.delete(entity.uuid)
            database.value.recentlyClosedTabDao().deleteTab(entity)
        }
    }

    @VisibleForTesting
    internal suspend fun addTabState(tab: RecoverableTab) {
        val entity = tab.state.toRecentlyClosedTabEntity()
        // Even if engine session state persistence fails, degrade gracefully by storing the tab
        // itself in the db - that will allow user to restore it with a "fresh" engine state.
        // That's a form of data loss, but not much we can do here other than log.
        tab.engineSessionState?.let {
            if (!engineStateStorage.write(entity.uuid, it)) {
                logger.warn("Failed to write engine session state for tab UUID = ${entity.uuid}")
            }
        }
        database.value.recentlyClosedTabDao().insertTab(entity)
    }
}
