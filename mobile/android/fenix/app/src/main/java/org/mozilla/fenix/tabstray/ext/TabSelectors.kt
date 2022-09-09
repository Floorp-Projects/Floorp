/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.ext.maxActiveTime

/**
 * The currently selected tab if there's one that is private.
 *
 * NB: Upstream to Selectors.kt.
 */
val BrowserState.selectedPrivateTab: TabSessionState?
    get() = selectedTabId?.let { id -> findPrivateTab(id) }

/**
 * Finds and returns the private  tab with the given id. Returns null if no
 * matching tab could be found.
 *
 * @param tabId The ID of the tab to search for.
 * @return The [TabSessionState] with the provided [tabId] or null if it could not be found.
 *
 * NB: Upstream to Selectors.kt.
 */
fun BrowserState.findPrivateTab(tabId: String): TabSessionState? {
    return privateTabs.firstOrNull { it.id == tabId }
}

/**
 * The list of normal tabs in the tabs tray filtered appropriately based on feature flags.
 */
fun BrowserState.getNormalTrayTabs(
    inactiveTabsEnabled: Boolean,
): List<TabSessionState> {
    return normalTabs.run {
        if (inactiveTabsEnabled) {
            filter { it.isNormalTabActive(maxActiveTime) }
        } else {
            this
        }
    }
}
