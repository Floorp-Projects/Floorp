/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import mozilla.components.browser.state.state.recover.RecoverableTab

/**
 * A restored browser state, read from disk.
 *
 * @param tabs The list of restored tabs.
 * @param selectedTabId The ID of the selected tab in [tabs]. Or `null` if no selection was restored.
 */
data class RecoverableBrowserState(
    val tabs: List<RecoverableTab>,
    val selectedTabId: String?,
)
