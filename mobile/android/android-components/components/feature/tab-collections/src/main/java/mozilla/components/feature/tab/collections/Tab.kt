/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine

/**
 * A tab of a [TabCollection].
 */
interface Tab {
    /**
     * Unique ID identifying this tab.
     */
    val id: Long

    /**
     * The title of the tab.
     */
    val title: String

    /**
     * The URL of the tab.
     */
    val url: String

    /**
     * Restores a single tab from this collection and returns a matching [Session].
     *
     * @param restoreSessionId If true the original [Session.id] of [Session]s will be restored. Otherwise a new ID
     * will be generated. An app may prefer to use a new ID if it expects sessions to get restored multiple times -
     * otherwise breaking the promise of a unique ID per [Session].
     */
    fun restore(
        context: Context,
        engine: Engine,
        tab: Tab,
        restoreSessionId: Boolean = false
    ): Session?
}
