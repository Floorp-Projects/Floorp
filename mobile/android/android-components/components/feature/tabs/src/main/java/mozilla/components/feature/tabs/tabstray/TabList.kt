/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import mozilla.components.browser.state.state.TabSessionState

internal data class TabList(
    val list: List<TabSessionState>,
    val selectedTabId: String?,
)
