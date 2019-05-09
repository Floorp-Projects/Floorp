/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections

import android.content.Context
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine

/**
 * A collection of tabs.
 */
interface TabCollection {
    /**
     * Unique ID of this tab collection.
     */
    val id: Long

    /**
     * Title of this tab collection.
     */
    val title: String

    /**
     * List of tabs in this tab collection.
     */
    val tabs: List<Tab>

    /**
     * Restores all tabs in this collection and returns a matching [SessionManager.Snapshot].
     */
    fun restore(context: Context, engine: Engine): SessionManager.Snapshot

    /**
     * Restores a subset of the tabs in this collection and returns a matching [SessionManager.Snapshot].
     */
    fun restoreSubset(context: Context, engine: Engine, tabs: List<Tab>): SessionManager.Snapshot
}
