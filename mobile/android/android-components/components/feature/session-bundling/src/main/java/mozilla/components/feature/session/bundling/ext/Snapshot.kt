package mozilla.components.feature.session.bundling.ext

import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.feature.session.bundling.db.UrlList

internal fun SessionManager.Snapshot.toBundleEntity() = BundleEntity(
    id = null,
    state = SnapshotSerializer().toJSON(this),
    savedAt = System.currentTimeMillis(),
    urls = UrlList(sessions.map { it.session.url })
)
