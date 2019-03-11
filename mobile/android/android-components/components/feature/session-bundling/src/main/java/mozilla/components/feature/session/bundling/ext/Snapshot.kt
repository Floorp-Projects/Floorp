/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.ext

import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.feature.session.bundling.db.UrlList

internal fun SessionManager.Snapshot.toBundleEntity() = BundleEntity(
    id = null,
    savedAt = System.currentTimeMillis(),
    urls = UrlList(sessions.map { it.session.url })
)
