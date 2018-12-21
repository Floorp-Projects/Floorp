package mozilla.components.feature.session.bundling.ext

import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.bundling.db.BundleEntity

internal fun SessionManager.Snapshot.toBundleEntity() = BundleEntity(
    id = null,
    state = "",
    savedAt = System.currentTimeMillis()
)
