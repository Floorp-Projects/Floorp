/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.ext

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.tabstray.Tabs

private fun BrowserState.mediaStateForTab(tab: TabSessionState): MediaState.State =
    if (media.aggregate.activeTabId == tab.id) {
        media.aggregate.state
    } else {
        MediaState.State.NONE
    }

internal fun BrowserState.toTabs(
    tabsFilter: (TabSessionState) -> Boolean = { true }
) = Tabs(
    list = tabs
        .filter(tabsFilter)
        .map { it.toTab(mediaStateForTab(it)) },
    selectedIndex = tabs
            .filter(tabsFilter)
            .indexOfFirst { it.id == selectedTabId }
)
