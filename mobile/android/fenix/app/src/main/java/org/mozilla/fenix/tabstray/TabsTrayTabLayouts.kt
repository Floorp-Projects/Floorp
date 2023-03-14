/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.tabstray.TabGridItem
import org.mozilla.fenix.compose.tabstray.TabListItem
import org.mozilla.fenix.tabstray.ext.MIN_COLUMN_WIDTH_DP
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Top-level UI for displaying a list of tabs.
 *
 * @param tabs The list of [TabSessionState] to display.
 */
@Composable
fun TabList(
    tabs: List<TabSessionState>,
) {
    val tabListBottomPadding = dimensionResource(id = R.dimen.tab_tray_list_bottom_padding)
    val state = rememberLazyListState()

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        state = state,
    ) {
        items(
            items = tabs,
            key = { tab -> tab.id },
        ) { tab ->
            TabListItem(
                tab = tab,
                onCloseClick = {},
                onMediaClick = {},
                onClick = {},
                onLongClick = {},
            )
        }

        item {
            Spacer(modifier = Modifier.height(tabListBottomPadding))
        }
    }
}

/**
 * Top-level UI for displaying a grid of tabs.
 *
 * @param tabs The list of [TabSessionState] to display.
 */
@Composable
fun TabGrid(
    tabs: List<TabSessionState>,
) {
    val tabListBottomPadding = dimensionResource(id = R.dimen.tab_tray_list_bottom_padding)
    val state = rememberLazyGridState()

    LazyVerticalGrid(
        columns = GridCells.Adaptive(minSize = MIN_COLUMN_WIDTH_DP.dp),
        modifier = Modifier.fillMaxSize(),
        state = state,
    ) {
        items(
            items = tabs,
            key = { tab -> tab.id },
        ) { tab ->
            TabGridItem(
                tab = tab,
                onCloseClick = {},
                onMediaClick = {},
                onClick = {},
                onLongClick = {},
            )
        }

        item(span = { GridItemSpan(maxLineSpan) }) {
            Spacer(modifier = Modifier.height(tabListBottomPadding))
        }
    }
}

@LightDarkPreview
@Composable
private fun TabListPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
        ) {
            TabList(
                tabs = generateFakeTabsList(),
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun TabGridPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
        ) {
            TabGrid(
                tabs = generateFakeTabsList(),
            )
        }
    }
}

private fun generateFakeTabsList(tabCount: Int = 10): List<TabSessionState> {
    val fakeTab = TabSessionState(
        id = "tabId",
        content = ContentState(
            url = "www.mozilla.com",
        ),
    )

    return List(tabCount) { fakeTab }
}
