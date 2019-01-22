/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import java.util.concurrent.TimeUnit

/**
 * A bundle of sessions and their state.
 */
interface SessionBundle {
    /**
     * A unique ID identifying this bundle. Can be `null` if this is a new Bundle that has not been saved to disk yet.
     */
    val id: Long?

    /**
     * Restore a [SessionManager.Snapshot] from this bundle. The returned snapshot can be used with [SessionManager] to
     * restore the sessions and their state.
     */
    fun restoreSnapshot(engine: Engine): SessionManager.Snapshot?

    /**
     * Returns the timestamp of the last time this bundle was saved in the provided [TimeUnit]. By default returns
     * milliseconds.
     */
    fun lastSavedAt(unit: TimeUnit = TimeUnit.MILLISECONDS): Long
}
