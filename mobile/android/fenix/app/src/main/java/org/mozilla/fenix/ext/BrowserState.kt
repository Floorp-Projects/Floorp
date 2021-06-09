/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState

/**
 * Returns the currently selected tab if there's one as a list.
 *
 * @return A list of the currently selected tab or an empty list.
 */
fun BrowserState.asRecentTabs(): List<TabSessionState> {
    val tab = selectedTab

    return if (tab != null && !tab.content.private) {
        listOfNotNull(tab)
    } else {
        emptyList()
    }
}
