/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import mozilla.components.browser.state.state.recover.RecoverableTab

/**
 * A restored browsing session, read from disk.
 */
data class BrowsingSession(
    val tabs: List<RecoverableTab>,
    val selectedTabId: String?
)
