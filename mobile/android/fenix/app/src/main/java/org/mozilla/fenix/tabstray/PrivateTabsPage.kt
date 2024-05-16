/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsComposableState

@Composable
@Suppress("LongParameterList")
internal fun PrivateTabsPage(
    browserStore: BrowserStore,
    tabsTrayStore: TabsTrayStore,
    displayTabsInGrid: Boolean,
    onTabClose: (TabSessionState) -> Unit,
    onTabMediaClick: (TabSessionState) -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabLongClick: (TabSessionState) -> Unit,
    onMove: (String, String?, Boolean) -> Unit,
) {
    val selectedTabId = browserStore
        .observeAsComposableState { state -> state.selectedTabId }.value
    val privateTabs = tabsTrayStore
        .observeAsComposableState { state -> state.privateTabs }.value ?: emptyList()
    val selectionMode = tabsTrayStore
        .observeAsComposableState { state -> state.mode }.value ?: TabsTrayState.Mode.Normal

    if (privateTabs.isNotEmpty()) {
        TabLayout(
            tabs = privateTabs,
            displayTabsInGrid = displayTabsInGrid,
            selectedTabId = selectedTabId,
            selectionMode = selectionMode,
            modifier = Modifier.testTag(TabsTrayTestTag.privateTabsList),
            onTabClose = onTabClose,
            onTabMediaClick = onTabMediaClick,
            onTabClick = onTabClick,
            onTabLongClick = onTabLongClick,
            onTabDragStart = {
                // Because we don't currently support selection mode for private tabs,
                // there's no need to exit selection mode when dragging tabs.
            },
            onMove = onMove,
        )
    } else {
        EmptyTabPage(isPrivate = true)
    }
}
