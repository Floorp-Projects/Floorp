/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.adapter

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.ext.readSnapshotItem
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tab.collections.db.TabCollectionWithTabs
import mozilla.components.feature.tab.collections.db.TabEntity

internal class TabCollectionAdapter(
    internal val entity: TabCollectionWithTabs
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

    override fun restore(context: Context, engine: Engine, restoreSessionId: Boolean): List<Session> {
        return restore(context, engine, entity.tabs, restoreSessionId)
    }

    override fun restoreSubset(
        context: Context,
        engine: Engine,
        tabs: List<Tab>,
        restoreSessionId: Boolean
    ): List<Session> {
        val entities = entity.tabs.filter {
            candidate -> tabs.find { tab -> tab.id == candidate.id } != null
        }
        return restore(context, engine, entities, restoreSessionId)
    }

    private fun restore(
        context: Context,
        engine: Engine,
        tabs: List<TabEntity>,
        restoreSessionId: Boolean
    ): List<Session> {
        return tabs.mapNotNull { tab ->
            tab.getStateFile(context.filesDir).readSnapshotItem(engine, restoreSessionId)?.session
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
