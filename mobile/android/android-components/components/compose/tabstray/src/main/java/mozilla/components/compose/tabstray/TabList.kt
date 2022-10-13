/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsComposableState

/**
 * Renders a list of tabs from the given [store], using the provided [tabsFilter].
 *
 * @param store The store to observe.
 * @param modifier The modifier to apply to this layout.
 * @param tabsFilter Used to filter the list of tabs from the [store].
 * @param onTabClosed Gets invoked when the user closes a tab.
 * @param onTabSelected Gets invoked when the user selects a tab.
 */
@Composable
fun TabList(
    store: BrowserStore,
    modifier: Modifier = Modifier,
    tabsFilter: (TabSessionState) -> Boolean = { true },
    onTabSelected: (TabSessionState) -> Unit = {},
    onTabClosed: (TabSessionState) -> Unit = {},
) {
    val tabs = store.observeAsComposableState { state -> state.tabs.filter(tabsFilter) }
    val selectedTabId = store.observeAsComposableState { state -> state.selectedTabId }
    TabList(
        tabs.value ?: emptyList(),
        modifier,
        selectedTabId.value,
        onTabSelected,
        onTabClosed,
    )
}

/**
 * Renders the given list of [tabs].
 *
 * @param tabs The list of tabs to render.
 * @param selectedTabId the currently selected tab ID.
 * @param modifier The modifier to apply to this layout.
 * @param onTabClosed Gets invoked when the user closes a tab.
 * @param onTabSelected Gets invoked when the user selects a tab.
 */
@Composable
fun TabList(
    tabs: List<TabSessionState>,
    modifier: Modifier = Modifier,
    selectedTabId: String? = null,
    onTabSelected: (TabSessionState) -> Unit,
    onTabClosed: (TabSessionState) -> Unit,
) {
    LazyColumn(
        modifier = modifier
            .fillMaxWidth()
            .background(MaterialTheme.colors.surface),
    ) {
        items(tabs) { tab ->
            Tab(
                tab,
                selected = selectedTabId == tab.id,
                onClick = { onTabSelected(tab) },
                onClose = { onTabClosed(tab) },
            )
        }
    }
}
