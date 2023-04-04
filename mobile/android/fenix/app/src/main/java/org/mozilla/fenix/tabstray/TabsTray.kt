/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.compose.ui.unit.dp
import com.google.accompanist.pager.ExperimentalPagerApi
import com.google.accompanist.pager.HorizontalPager
import com.google.accompanist.pager.rememberPagerState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.tabstray.ext.isNormalTab
import org.mozilla.fenix.tabstray.inactivetabs.InactiveTabsList
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Top-level UI for displaying the Tabs Tray feature.
 *
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 * @param displayTabsInGrid Whether the normal and private tabs should be displayed in a grid.
 * @param onTabClose Invoked when the user clicks to close a tab.
 * @param onTabMediaClick Invoked when the user interacts with a tab's media controls.
 * @param onTabClick Invoked when the user clicks on a tab.
 * @param onTabMultiSelectClick Invoked when the user clicks on a tab while in multi-select mode.
 * @param onTabLongClick Invoked when the user long clicks a tab.
 * @param onInactiveTabsHeaderClick Invoked when the user clicks on the inactive tabs section header.
 * @param onDeleteAllInactiveTabsClick Invoked when the user clicks on the delete all inactive tabs button.
 * @param onInactiveTabsAutoCloseDialogShown Invoked when the inactive tabs auto close dialog
 * is presented to the user.
 * @param onInactiveTabAutoCloseDialogCloseButtonClick Invoked when the user clicks on the inactive
 * tab auto close dialog's dismiss button.
 * @param onEnableInactiveTabAutoCloseClick Invoked when the user clicks on the inactive tab auto
 * close dialog's enable button.
 * @param onInactiveTabClick Invoked when the user clicks on an inactive tab.
 * @param onInactiveTabClose Invoked when the user clicks on an inactive tab's close button.
 */
@OptIn(ExperimentalPagerApi::class, ExperimentalComposeUiApi::class)
@Suppress("LongMethod", "LongParameterList")
@Composable
fun TabsTray(
    appStore: AppStore,
    tabsTrayStore: TabsTrayStore,
    displayTabsInGrid: Boolean,
    shouldShowInactiveTabsAutoCloseDialog: (Int) -> Boolean,
    onTabPageClick: (Page) -> Unit,
    onTabClose: (TabSessionState) -> Unit,
    onTabMediaClick: (TabSessionState) -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabMultiSelectClick: (TabSessionState) -> Unit,
    onTabLongClick: (TabSessionState) -> Unit,
    onInactiveTabsHeaderClick: (Boolean) -> Unit,
    onDeleteAllInactiveTabsClick: () -> Unit,
    onInactiveTabsAutoCloseDialogShown: () -> Unit,
    onInactiveTabAutoCloseDialogCloseButtonClick: () -> Unit,
    onEnableInactiveTabAutoCloseClick: () -> Unit,
    onInactiveTabClick: (TabSessionState) -> Unit,
    onInactiveTabClose: (TabSessionState) -> Unit,
) {
    val multiselectMode = tabsTrayStore
        .observeAsComposableState { state -> state.mode }.value ?: TabsTrayState.Mode.Normal
    val selectedPage = tabsTrayStore
        .observeAsComposableState { state -> state.selectedPage }.value ?: Page.NormalTabs
    val normalTabs = tabsTrayStore
        .observeAsComposableState { state -> state.normalTabs }.value ?: emptyList()
    val privateTabs = tabsTrayStore
        .observeAsComposableState { state -> state.privateTabs }.value ?: emptyList()
    val inactiveTabsExpanded = appStore
        .observeAsComposableState { state -> state.inactiveTabsExpanded }.value ?: false
    val inactiveTabs = tabsTrayStore
        .observeAsComposableState { state -> state.inactiveTabs }.value ?: emptyList()
    val pagerState = rememberPagerState(initialPage = selectedPage.ordinal)
    val isInMultiSelectMode = multiselectMode is TabsTrayState.Mode.Select

    val handleTabClick: ((TabSessionState) -> Unit) = { tab ->
        if (isInMultiSelectMode) {
            onTabMultiSelectClick(tab)
        } else {
            onTabClick(tab)
        }
    }

    LaunchedEffect(selectedPage) {
        pagerState.animateScrollToPage(selectedPage.ordinal)
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .clip(RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp))
            .background(FirefoxTheme.colors.layer1),
    ) {
        Box(modifier = Modifier.nestedScroll(rememberNestedScrollInteropConnection())) {
            TabsTrayBanner(
                isInMultiSelectMode = isInMultiSelectMode,
                selectedPage = selectedPage,
                normalTabCount = normalTabs.size + inactiveTabs.size,
                onTabPageIndicatorClicked = onTabPageClick,
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
                        val showInactiveTabsAutoCloseDialog = shouldShowInactiveTabsAutoCloseDialog(inactiveTabs.size)
                        var showAutoCloseDialog by remember { mutableStateOf(showInactiveTabsAutoCloseDialog) }

                        val optionalInactiveTabsHeader: (@Composable () -> Unit)? = if (inactiveTabs.isEmpty()) {
                            null
                        } else {
                            {
                                InactiveTabsList(
                                    inactiveTabs = inactiveTabs,
                                    expanded = inactiveTabsExpanded,
                                    showAutoCloseDialog = showAutoCloseDialog,
                                    onHeaderClick = onInactiveTabsHeaderClick,
                                    onDeleteAllButtonClick = onDeleteAllInactiveTabsClick,
                                    onAutoCloseDismissClick = {
                                        onInactiveTabAutoCloseDialogCloseButtonClick()
                                        showAutoCloseDialog = !showAutoCloseDialog
                                    },
                                    onEnableAutoCloseClick = {
                                        onEnableInactiveTabAutoCloseClick()
                                        showAutoCloseDialog = !showAutoCloseDialog
                                    },
                                    onTabClick = onInactiveTabClick,
                                    onTabCloseClick = onInactiveTabClose,
                                )
                            }
                        }

                        if (showInactiveTabsAutoCloseDialog) {
                            onInactiveTabsAutoCloseDialogShown()
                        }

                        TabLayout(
                            tabs = normalTabs,
                            displayTabsInGrid = displayTabsInGrid,
                            onTabClose = onTabClose,
                            onTabMediaClick = onTabMediaClick,
                            onTabClick = handleTabClick,
                            onTabLongClick = onTabLongClick,
                            header = optionalInactiveTabsHeader,
                        )
                    }
                    Page.PrivateTabs -> {
                        TabLayout(
                            tabs = privateTabs,
                            displayTabsInGrid = displayTabsInGrid,
                            onTabClose = onTabClose,
                            onTabMediaClick = onTabMediaClick,
                            onTabClick = handleTabClick,
                            onTabLongClick = onTabLongClick,
                        )
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
    TabsTrayPreviewRoot(
        displayTabsInGrid = false,
        normalTabs = generateFakeTabsList(),
        privateTabs = generateFakeTabsList(
            tabCount = 7,
            isPrivate = true,
        ),
    )
}

@LightDarkPreview
@Composable
private fun TabsTrayMultiSelectPreview() {
    TabsTrayPreviewRoot(
        mode = TabsTrayState.Mode.Select(setOf()),
        normalTabs = generateFakeTabsList(),
    )
}

@LightDarkPreview
@Composable
private fun TabsTrayInactiveTabsPreview() {
    TabsTrayPreviewRoot(
        normalTabs = generateFakeTabsList(tabCount = 3),
        inactiveTabs = generateFakeTabsList(),
        inactiveTabsExpanded = true,
        showInactiveTabsAutoCloseDialog = true,
    )
}

@LightDarkPreview
@Composable
private fun TabsTrayPrivateTabsPreview() {
    TabsTrayPreviewRoot(
        selectedPage = Page.PrivateTabs,
        privateTabs = generateFakeTabsList(),
    )
}

@LightDarkPreview
@Composable
private fun TabsTraySyncedTabsPreview() {
    TabsTrayPreviewRoot(
        selectedPage = Page.SyncedTabs,
    )
}

@Composable
private fun TabsTrayPreviewRoot(
    displayTabsInGrid: Boolean = true,
    selectedPage: Page = Page.NormalTabs,
    mode: TabsTrayState.Mode = TabsTrayState.Mode.Normal,
    normalTabs: List<TabSessionState> = emptyList(),
    inactiveTabs: List<TabSessionState> = emptyList(),
    privateTabs: List<TabSessionState> = emptyList(),
    inactiveTabsExpanded: Boolean = false,
    showInactiveTabsAutoCloseDialog: Boolean = false,
) {
    var selectedPageState by remember { mutableStateOf(selectedPage) }
    val normalTabsState = remember { normalTabs.toMutableStateList() }
    val inactiveTabsState = remember { inactiveTabs.toMutableStateList() }
    val privateTabsState = remember { privateTabs.toMutableStateList() }
    var inactiveTabsExpandedState by remember { mutableStateOf(inactiveTabsExpanded) }
    var showInactiveTabsAutoCloseDialogState by remember { mutableStateOf(showInactiveTabsAutoCloseDialog) }

    val appStore = AppStore(
        initialState = AppState(
            inactiveTabsExpanded = inactiveTabsExpandedState,
        ),
    )
    val tabsTrayStore = TabsTrayStore(
        initialState = TabsTrayState(
            selectedPage = selectedPageState,
            mode = mode,
            inactiveTabs = inactiveTabsState,
            normalTabs = normalTabsState,
            privateTabs = privateTabs,
        ),
    )

    FirefoxTheme {
        TabsTray(
            appStore = appStore,
            tabsTrayStore = tabsTrayStore,
            displayTabsInGrid = displayTabsInGrid,
            shouldShowInactiveTabsAutoCloseDialog = { true },
            onTabPageClick = { page ->
                selectedPageState = page
            },
            onTabClose = { tab ->
                if (tab.isNormalTab()) {
                    normalTabsState.remove(tab)
                } else {
                    privateTabsState.remove(tab)
                }
            },
            onTabMediaClick = {},
            onTabClick = {},
            onTabMultiSelectClick = {},
            onTabLongClick = {},
            onInactiveTabsHeaderClick = {
                inactiveTabsExpandedState = !inactiveTabsExpandedState
            },
            onDeleteAllInactiveTabsClick = inactiveTabsState::clear,
            onInactiveTabsAutoCloseDialogShown = {},
            onInactiveTabAutoCloseDialogCloseButtonClick = {
                showInactiveTabsAutoCloseDialogState = !showInactiveTabsAutoCloseDialogState
            },
            onEnableInactiveTabAutoCloseClick = {
                showInactiveTabsAutoCloseDialogState = !showInactiveTabsAutoCloseDialogState
            },
            onInactiveTabClick = {},
            onInactiveTabClose = inactiveTabsState::remove,
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
