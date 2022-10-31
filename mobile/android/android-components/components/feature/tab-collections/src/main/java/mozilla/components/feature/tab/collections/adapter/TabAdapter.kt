/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.adapter

import android.content.Context
import mozilla.components.browser.session.storage.serialize.BrowserStateReader
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.db.TabEntity

internal class TabAdapter(
    val entity: TabEntity,
) : Tab {
    override val id: Long
        get() = entity.id!!

    override val title: String
        get() = entity.title

    override val url: String
        get() = entity.url

    override fun restore(
        context: Context,
        engine: Engine,
        restoreSessionId: Boolean,
    ): RecoverableTab? {
        val reader = BrowserStateReader()
        val file = entity.getStateFile(context.filesDir)
        return reader.readTab(engine, file, restoreSessionId, restoreParentId = false)
    }

    override fun equals(other: Any?): Boolean {
        if (other !is TabAdapter) {
            return false
        }

        return entity == other.entity
    }

    override fun hashCode(): Int {
        return entity.hashCode()
    }
}
