/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.ext

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.tabs.tabstray.Tabs

/**
 * Converts the tabs in [BrowserState], using [tabsFilter], to a [Tabs] object.
 *
 * This implementation is use to help observe changes to the [BrowserState] tabs when only a select
 * few properties have changed.
 */
internal fun BrowserState.toTabs(
    tabsFilter: (TabSessionState) -> Boolean = { true },
): Tabs {
    val (tabStates, selectedTabId) = toTabList(tabsFilter)
    val tabs = tabStates.map { it.toTab() }
    return Tabs(tabs, selectedTabId)
}

/**
 * Returns a list of tabs with the applied [tabsFilter] and the selected tab ID.
 */
internal fun BrowserState.toTabList(
    tabsFilter: (TabSessionState) -> Boolean = { true },
): Pair<List<TabSessionState>, String?> {
    val tabStates = tabs.filter(tabsFilter)
    val selectedTabId = tabStates
        .filter(tabsFilter)
        .firstOrNull { it.id == selectedTabId }
        ?.id

    return Pair(tabStates, selectedTabId)
}
