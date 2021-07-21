/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.feature.tabs.ext.toTabs
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
    onTabSelected: (Tab) -> Unit = {},
    onTabClosed: (Tab) -> Unit = {}
) {
    val tabs = store.observeAsComposableState { state -> state.toTabs(tabsFilter) }
    TabList(
        tabs.value ?: Tabs(emptyList(), selectedIndex = -1),
        onTabSelected,
        onTabClosed,
        modifier
    )
}

/**
 * Renders the given list of [tabs].
 *
 * @param tabs The list of tabs to render.
 * @param modifier The modifier to apply to this layout.
 * @param onTabClosed Gets invoked when the user closes a tab.
 * @param onTabSelected Gets invoked when the user selects a tab.
 */
@Composable
fun TabList(
    tabs: Tabs,
    onTabSelected: (Tab) -> Unit,
    onTabClosed: (Tab) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyColumn(
        modifier = modifier
            .fillMaxWidth()
            .background(MaterialTheme.colors.surface)
    ) {
        itemsIndexed(tabs.list) { index, tab ->
            Tab(
                tab,
                selected = tabs.selectedIndex == index,
                onClick = { onTabSelected(tab) },
                onClose = { onTabClosed(tab) }
            )
        }
    }
}
