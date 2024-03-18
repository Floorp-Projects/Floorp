/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore

/**
 * Find and extract a list [TabSessionState] from the [BrowserStore] using the IDs from [tabs].
 */
fun BrowserStore.getTabSessionState(tabs: Collection<TabSessionState>): List<TabSessionState> {
    return tabs.mapNotNull {
        state.findTab(it.id)
    }
}
