/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyListItemInfo
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.systemGestureExclusion
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Devices
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.coerceIn
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

private val minTabStripItemWidth = 160.dp
private val maxTabStripItemWidth = 280.dp
private val tabStripIconSize = 24.dp
private val spaceBetweenTabs = 4.dp
private val tabStripStartPadding = 8.dp
private val addTabIconSize = 20.dp

/**
 * Top level composable for the tabs strip.
 *
 * @param onHome Whether or not the tabs strip is in the home screen.
 * @param browserStore The [BrowserStore] instance used to observe tabs state.
 * @param appStore The [AppStore] instance used to observe browsing mode.
 * @param tabsUseCases The [TabsUseCases] instance to perform tab actions.
 * @param onAddTabClick Invoked when the add tab button is clicked.
 * @param onLastTabClose Invoked when the last remaining open tab is closed.
 * @param onSelectedTabClick Invoked when a tab is selected.
 */
@Composable
fun TabStrip(
    onHome: Boolean = false,
    browserStore: BrowserStore = components.core.store,
    appStore: AppStore = components.appStore,
    tabsUseCases: TabsUseCases = components.useCases.tabsUseCases,
    onAddTabClick: () -> Unit,
    onLastTabClose: () -> Unit,
    onSelectedTabClick: () -> Unit,
) {
    val isPrivateMode by appStore.observeAsState(false) { it.mode.isPrivate }
    val state by browserStore.observeAsState(TabStripState.initial) {
        it.toTabStripState(isSelectDisabled = onHome, isPrivateMode = isPrivateMode)
    }

    TabStripContent(
        state = state,
        onAddTabClick = onAddTabClick,
        onCloseTabClick = {
            if (state.tabs.size == 1) {
                onLastTabClose()
            }
            tabsUseCases.removeTab(it)
        },
        onSelectedTabClick = {
            tabsUseCases.selectTab(it)
            onSelectedTabClick()
        },
    )
}

@Composable
private fun TabStripContent(
    state: TabStripState,
    onAddTabClick: () -> Unit,
    onCloseTabClick: (id: String) -> Unit,
    onSelectedTabClick: (id: String) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer1)
            .systemGestureExclusion(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        TabsList(
            state = state,
            modifier = Modifier.weight(1f, fill = false),
            onCloseTabClick = onCloseTabClick,
            onSelectedTabClick = onSelectedTabClick,
        )

        IconButton(onClick = onAddTabClick) {
            Icon(
                painter = painterResource(R.drawable.mozac_ic_plus_24),
                modifier = Modifier.size(addTabIconSize),
                tint = FirefoxTheme.colors.iconPrimary,
                contentDescription = stringResource(R.string.add_tab),
            )
        }
    }
}

@Composable
@OptIn(ExperimentalFoundationApi::class)
private fun TabsList(
    state: TabStripState,
    modifier: Modifier = Modifier,
    onCloseTabClick: (id: String) -> Unit,
    onSelectedTabClick: (id: String) -> Unit,
) {
    BoxWithConstraints(modifier = modifier) {
        val listState = rememberLazyListState()
        // Calculate the width of each tab item based on available width and the number of tabs and
        // taking into account the space between tabs.
        val availableWidth = maxWidth - tabStripStartPadding
        val tabWidth = (availableWidth / state.tabs.size) - spaceBetweenTabs

        LazyRow(
            modifier = Modifier,
            state = listState,
            contentPadding = PaddingValues(start = tabStripStartPadding),
        ) {
            items(
                items = state.tabs,
                key = { it.id },
            ) { itemState ->
                TabItem(
                    state = itemState,
                    onCloseTabClick = onCloseTabClick,
                    onSelectedTabClick = onSelectedTabClick,
                    modifier = Modifier
                        .padding(end = spaceBetweenTabs)
                        .animateItemPlacement()
                        .width(
                            tabWidth.coerceIn(
                                minimumValue = minTabStripItemWidth,
                                maximumValue = maxTabStripItemWidth,
                            ),
                        ),
                )
            }
        }

        if (state.tabs.isNotEmpty()) {
            // When a new tab is added, scroll to the end of the list. This is done here instead of
            // in onCloseTabClick so this acts on state change which can occur from any other
            // place e.g. tabs tray.
            LaunchedEffect(state.tabs.last().id) {
                listState.scrollToItem(state.tabs.size)
            }

            // When a tab is selected, scroll to the selected tab. This is done here instead of
            // in onSelectedTabClick so this acts on state change which can occur from any other
            // place e.g. tabs tray.
            val selectedTab = state.tabs.firstOrNull { it.isSelected }
            LaunchedEffect(selectedTab?.id) {
                if (selectedTab != null) {
                    val selectedItemInfo =
                        listState.layoutInfo.visibleItemsInfo.firstOrNull { it.key == selectedTab.id }

                    if (listState.isItemPartiallyVisible(selectedItemInfo) || selectedItemInfo == null) {
                        listState.animateScrollToItem(state.tabs.indexOf(selectedTab))
                    }
                }
            }
        }
    }
}

private fun LazyListState.isItemPartiallyVisible(itemInfo: LazyListItemInfo?) =
    itemInfo != null &&
        (itemInfo.offset + itemInfo.size > layoutInfo.viewportEndOffset || itemInfo.offset < 0)

@Composable
private fun TabItem(
    state: TabStripItem,
    modifier: Modifier = Modifier,
    onCloseTabClick: (id: String) -> Unit,
    onSelectedTabClick: (id: String) -> Unit,
) {
    TabStripCard(
        modifier = modifier.fillMaxSize(),
        backgroundColor =
        if (state.isPrivate) {
            if (state.isSelected) {
                FirefoxTheme.colors.layer3
            } else {
                FirefoxTheme.colors.layer2
            }
        } else {
            if (state.isSelected) {
                FirefoxTheme.colors.layer2
            } else {
                FirefoxTheme.colors.layer3
            }
        },
        elevation = if (state.isSelected) {
            selectedTabStripCardElevation
        } else {
            defaultTabStripCardElevation
        },
    ) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .clickable { onSelectedTabClick(state.id) },
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Row(
                modifier = Modifier.weight(1f, fill = false),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Spacer(modifier = Modifier.size(8.dp))

                TabStripIcon(state.url)

                Spacer(modifier = Modifier.size(8.dp))

                Text(
                    text = state.title,
                    color = FirefoxTheme.colors.textPrimary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    style = FirefoxTheme.typography.subtitle2,
                )
            }

            IconButton(onClick = { onCloseTabClick(state.id) }) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_cross_20),
                    tint = FirefoxTheme.colors.iconPrimary,
                    contentDescription = stringResource(R.string.close_tab),
                )
            }
        }
    }
}

@Composable
private fun TabStripIcon(url: String) {
    Box(
        modifier = Modifier
            .size(tabStripIconSize)
            .clip(CircleShape),
        contentAlignment = Alignment.Center,
    ) {
        Favicon(
            url = url,
            size = tabStripIconSize,
        )
    }
}

private class TabUIStateParameterProvider : PreviewParameterProvider<TabStripState> {
    override val values: Sequence<TabStripState>
        get() = sequenceOf(
            TabStripState(
                listOf(
                    TabStripItem(
                        id = "1",
                        title = "Tab 1",
                        url = "https://www.mozilla.org",
                        isPrivate = false,
                        isSelected = false,
                    ),
                    TabStripItem(
                        id = "2",
                        title = "Tab 2 with a very long title that should be truncated",
                        url = "https://www.mozilla.org",
                        isPrivate = false,
                        isSelected = false,
                    ),
                    TabStripItem(
                        id = "3",
                        title = "Selected tab",
                        url = "https://www.mozilla.org",
                        isPrivate = false,
                        isSelected = true,
                    ),
                    TabStripItem(
                        id = "p1",
                        title = "Private tab 1",
                        url = "https://www.mozilla.org",
                        isPrivate = true,
                        isSelected = false,
                    ),
                    TabStripItem(
                        id = "p2",
                        title = "Private selected tab",
                        url = "https://www.mozilla.org",
                        isPrivate = true,
                        isSelected = true,
                    ),
                ),
            ),
        )
}

@Preview(device = Devices.TABLET)
@Composable
private fun TabStripPreview(
    @PreviewParameter(TabUIStateParameterProvider::class) tabStripState: TabStripState,
) {
    FirefoxTheme {
        TabStripContentPreview(tabStripState.tabs.filter { !it.isPrivate })
    }
}

@Preview(device = Devices.TABLET)
@Composable
private fun TabStripPreviewDarkMode(
    @PreviewParameter(TabUIStateParameterProvider::class) tabStripState: TabStripState,
) {
    FirefoxTheme(theme = Theme.Dark) {
        TabStripContentPreview(tabStripState.tabs.filter { !it.isPrivate })
    }
}

@Preview(device = Devices.TABLET)
@Composable
private fun TabStripPreviewPrivateMode(
    @PreviewParameter(TabUIStateParameterProvider::class) tabStripState: TabStripState,
) {
    FirefoxTheme(theme = Theme.Private) {
        TabStripContentPreview(tabStripState.tabs.filter { it.isPrivate })
    }
}

@Composable
private fun TabStripContentPreview(tabs: List<TabStripItem>) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(dimensionResource(id = R.dimen.tab_strip_height)),
        contentAlignment = Alignment.Center,
    ) {
        TabStripContent(
            state = TabStripState(
                tabs = tabs,
            ),
            onAddTabClick = {},
            onCloseTabClick = {},
            onSelectedTabClick = {},
        )
    }
}

@Preview(device = Devices.TABLET)
@Composable
private fun TabStripPreview() {
    val browserStore = BrowserStore()

    FirefoxTheme {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(dimensionResource(id = R.dimen.tab_strip_height)),
            contentAlignment = Alignment.Center,
        ) {
            TabStrip(
                appStore = AppStore(),
                browserStore = browserStore,
                tabsUseCases = TabsUseCases(browserStore),
                onAddTabClick = {
                    val tab = createTab(
                        url = "www.example.com",
                    )
                    browserStore.dispatch(TabListAction.AddTabAction(tab))
                },
                onLastTabClose = {},
                onSelectedTabClick = {},
            )
        }
    }
}
