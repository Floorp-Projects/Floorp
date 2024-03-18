/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections

import android.content.Context
import mozilla.components.browser.state.state.recover.RecoverableTab
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
     * Restores all tabs in this collection and returns a matching list of [RecoverableTab] objects.
     *
     * @param restoreSessionId If true the original ID of the tabs will be restored. Otherwise a new ID
     * will be generated. An app may prefer to use a new ID if it expects tab to get restored multiple times -
     * otherwise breaking the promise of a unique ID per tab.
     */
    fun restore(
        context: Context,
        engine: Engine,
        restoreSessionId: Boolean = false,
    ): List<RecoverableTab>

    /**
     * Restores a subset of the tabs in this collection and returns a matching list of
     * [RecoverableTab] objects.
     *
     * @param restoreSessionId If true the original ID of the tabs will be restored. Otherwise a new ID
     * will be generated. An app may prefer to use a new ID if it expects tab to get restored multiple times -
     * otherwise breaking the promise of a unique ID per tab.
     */
    fun restoreSubset(
        context: Context,
        engine: Engine,
        tabs: List<Tab>,
        restoreSessionId: Boolean = false,
    ): List<RecoverableTab>
}
