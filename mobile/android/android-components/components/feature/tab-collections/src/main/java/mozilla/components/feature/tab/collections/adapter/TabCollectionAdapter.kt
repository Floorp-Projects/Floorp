/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.adapter

import android.content.Context
import mozilla.components.browser.session.storage.serialize.BrowserStateReader
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tab.collections.db.TabCollectionWithTabs
import mozilla.components.feature.tab.collections.db.TabEntity

internal class TabCollectionAdapter(
    internal val entity: TabCollectionWithTabs,
) : TabCollection {
    override val title: String
        get() = entity.collection.title

    override val tabs: List<Tab> by lazy {
        entity
            .tabs
            .sortedByDescending { it.createdAt }
            .map { TabAdapter(it) }
    }

    override val id: Long
        get() = entity.collection.id!!

    override fun restore(
        context: Context,
        engine: Engine,
        restoreSessionId: Boolean,
    ): List<RecoverableTab> {
        return restore(context, engine, entity.tabs, restoreSessionId)
    }

    override fun restoreSubset(
        context: Context,
        engine: Engine,
        tabs: List<Tab>,
        restoreSessionId: Boolean,
    ): List<RecoverableTab> {
        val entities = entity.tabs.filter {
                candidate ->
            tabs.find { tab -> tab.id == candidate.id } != null
        }
        return restore(context, engine, entities, restoreSessionId)
    }

    private fun restore(
        context: Context,
        engine: Engine,
        tabs: List<TabEntity>,
        restoreSessionId: Boolean,
    ): List<RecoverableTab> {
        val reader = BrowserStateReader()
        return tabs.mapNotNull { tab ->
            val file = tab.getStateFile(context.filesDir)
            reader.readTab(engine, file, restoreSessionId, restoreParentId = false)
        }
    }

    override fun equals(other: Any?): Boolean {
        if (other !is TabCollectionAdapter) {
            return false
        }

        return entity == other.entity
    }

    override fun hashCode(): Int {
        return entity.hashCode()
    }
}
