/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling

import androidx.annotation.WorkerThread
import mozilla.components.browser.session.SessionManager

/**
 * A bundle of sessions and their state.
 */
interface SessionBundle {
    /**
     * A unique ID identifying this bundle. Can be `null` if this is a new Bundle that has not been saved to disk yet.
     */
    val id: Long?

    /**
     * List of URLs saved in this bundle.
     */
    val urls: List<String>

    /**
     * Timestamp of the last time this bundle was saved (in milliseconds)
     */
    val lastSavedAt: Long

    /**
     * Restore a [SessionManager.Snapshot] from this bundle. The returned snapshot can be used with [SessionManager] to
     * restore the sessions and their state.
     */
    @WorkerThread
    fun restoreSnapshot(): SessionManager.Snapshot?
}
