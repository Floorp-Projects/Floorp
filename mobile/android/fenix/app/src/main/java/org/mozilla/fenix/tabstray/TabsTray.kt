/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.compose.ui.unit.dp
import com.google.accompanist.pager.ExperimentalPagerApi
import com.google.accompanist.pager.HorizontalPager
import com.google.accompanist.pager.rememberPagerState
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Top-level UI for displaying the Tabs Tray feature.
 *
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 * @param displayTabsInGrid Whether the normal and private tabs should be displayed in a grid.
 */
@OptIn(ExperimentalPagerApi::class, ExperimentalComposeUiApi::class)
@Composable
fun TabsTray(
    tabsTrayStore: TabsTrayStore,
    displayTabsInGrid: Boolean,
) {
    val multiselectMode = tabsTrayStore
        .observeAsComposableState { state -> state.mode }.value ?: TabsTrayState.Mode.Normal
    val normalTabs = tabsTrayStore
        .observeAsComposableState { state -> state.normalTabs }.value ?: emptyList()
    val privateTabs = tabsTrayStore
        .observeAsComposableState { state -> state.privateTabs }.value ?: emptyList()
    val pagerState = rememberPagerState(initialPage = 0)
    val scope = rememberCoroutineScope()
    val animateScrollToPage: ((Page) -> Unit) = { page ->
        scope.launch {
            pagerState.animateScrollToPage(page.ordinal)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer1),
    ) {
        Box(modifier = Modifier.nestedScroll(rememberNestedScrollInteropConnection())) {
            TabsTrayBanner(
                isInMultiSelectMode = multiselectMode is TabsTrayState.Mode.Select,
                onTabPageIndicatorClicked = animateScrollToPage,
            )
        }

        Divider()

        Box(modifier = Modifier.fillMaxSize()) {
            HorizontalPager(
                count = Page.values().size,
                modifier = Modifier.fillMaxSize(),
                state = pagerState,
                userScrollEnabled = false,
            ) { position ->
                when (Page.positionToPage(position)) {
                    Page.NormalTabs -> {
                        if (displayTabsInGrid) {
                            TabGrid(
                                tabs = normalTabs,
                            )
                        } else {
                            TabList(
                                tabs = normalTabs,
                            )
                        }
                    }
                    Page.PrivateTabs -> {
                        if (displayTabsInGrid) {
                            TabGrid(
                                tabs = privateTabs,
                            )
                        } else {
                            TabList(
                                tabs = privateTabs,
                            )
                        }
                    }
                    Page.SyncedTabs -> {
                        Text(
                            text = "Synced tabs",
                            modifier = Modifier.padding(all = 16.dp),
                            color = FirefoxTheme.colors.textPrimary,
                            style = FirefoxTheme.typography.body1,
                        )
                    }
                }
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayPreview() {
    val store = TabsTrayStore(
        initialState = TabsTrayState(
            normalTabs = generateFakeTabsList(),
            privateTabs = generateFakeTabsList(
                tabCount = 7,
                isPrivate = true,
            ),
        ),
    )

    FirefoxTheme {
        TabsTray(
            tabsTrayStore = store,
            displayTabsInGrid = false,
        )
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayMultiSelectPreview() {
    val store = TabsTrayStore(
        initialState = TabsTrayState(
            mode = TabsTrayState.Mode.Select(setOf()),
            normalTabs = generateFakeTabsList(),
        ),
    )

    FirefoxTheme {
        TabsTray(
            tabsTrayStore = store,
            displayTabsInGrid = true,
        )
    }
}

private fun generateFakeTabsList(tabCount: Int = 10, isPrivate: Boolean = false): List<TabSessionState> =
    List(tabCount) { index ->
        TabSessionState(
            id = "tabId$index-$isPrivate",
            content = ContentState(
                url = "www.mozilla.com",
                private = isPrivate,
            ),
        )
    }
