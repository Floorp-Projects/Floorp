/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.ClosedTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.recentlyclosed.db.RecentlyClosedTabsDatabase
import mozilla.components.feature.recentlyclosed.db.toRecentlyClosedTabEntity
import mozilla.components.support.ktx.java.io.truncateDirectory
import mozilla.components.support.ktx.util.streamJSON
import java.io.File

/**
 * A storage implementation that saves snapshots of recently closed tabs / sessions.
 */
@Suppress("TooManyFunctions")
internal class RecentlyClosedTabsStorage(
    context: Context,
    private val engine: Engine,
    private val scope: CoroutineScope = CoroutineScope(IO)
) : RecentlyClosedMiddleware.Storage {
    private val filesDir by lazy { context.filesDir }

    @VisibleForTesting
    internal var database: Lazy<RecentlyClosedTabsDatabase> =
        lazy { RecentlyClosedTabsDatabase.get(context) }

    /**
     * Returns an observable list of [ClosedTab]s.
     */
    override fun getTabs(): Flow<List<ClosedTab>> {
        return database.value.recentlyClosedTabDao().getTabs().map { list ->
            list.map { it.toClosedTab(filesDir, engine) }
        }
    }

    /**
     * Removes the given [ClosedTab].
     */
    override fun removeTab(recentlyClosedTab: ClosedTab) {
        val entity = recentlyClosedTab.toRecentlyClosedTabEntity()
        entity.getStateFile(filesDir).delete()
        database.value.recentlyClosedTabDao().deleteTab(entity)
    }

    /**
     * Removes all [ClosedTab]s.
     */
    override fun removeAllTabs() {
        getStateDirectory(filesDir).truncateDirectory()
        database.value.recentlyClosedTabDao().removeAllTabs()
    }

    private fun getStateDirectory(filesDir: File): File {
        return File(filesDir, "mozac.feature.recentlyclosed").apply {
            mkdirs()
        }
    }

    /**
     * Adds up to [maxTabs] [TabSessionState]s to storage, and then prunes storage to keep only the newest [maxTabs].
     */
    override fun addTabsToCollectionWithMax(
        tab: List<ClosedTab>,
        maxTabs: Int
    ) {
        tab.takeLast(maxTabs).forEach {
            addTabState(it)
        }
        pruneTabsWithMax(maxTabs)
    }

    private fun pruneTabsWithMax(maxTabs: Int) = scope.launch {
        database.value.recentlyClosedTabDao().getTabs().collect {
            // No pruning required
            if (it.size <= maxTabs) return@collect

            it.subList(maxTabs, it.size).forEach { entity ->
                entity.getStateFile(filesDir).delete()
                database.value.recentlyClosedTabDao().deleteTab(entity)
            }
        }
    }

    @VisibleForTesting
    internal fun addTabState(
        tab: ClosedTab
    ) {
        val entity = tab.toRecentlyClosedTabEntity()

        val success = entity.getStateFile(filesDir).streamJSON {
            val state = tab.engineSessionState
            if (state == null) {
                beginObject().endObject()
            } else {
                state.writeTo(this)
            }
        }

        if (success) {
            database.value.recentlyClosedTabDao().insertTab(entity)
        }
    }
}
