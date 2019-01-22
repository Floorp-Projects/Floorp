/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.adapter

import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.bundling.SessionBundle
import mozilla.components.feature.session.bundling.db.BundleEntity
import org.json.JSONException

/**
 * Adapter implementation to make a [BundleEntity] accessible as [SessionBundle] without "leaking" the underlying
 * entity class.
 */
internal class SessionBundleAdapter(
    internal val actual: BundleEntity
) : SessionBundle {

    override val id: Long?
        get() = actual.id

    override val urls: List<String>
        get() = actual.urls.entries

    override val lastSavedAt: Long
        get() = actual.savedAt

    /**
     * Re-create the [SessionManager.Snapshot] from the state saved in the database.
     */
    override fun restoreSnapshot(engine: Engine): SessionManager.Snapshot? {
        return try {
            SnapshotSerializer().fromJSON(engine, actual.state)
        } catch (e: JSONException) {
            null
        }
    }
}
