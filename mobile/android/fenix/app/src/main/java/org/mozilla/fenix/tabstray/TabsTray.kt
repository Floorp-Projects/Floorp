/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:OptIn(ExperimentalFoundationApi::class)

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.tabstray.ext.isNormalTab
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListItem
import org.mozilla.fenix.theme.FirefoxTheme
import mozilla.components.browser.storage.sync.Tab as SyncTab
import org.mozilla.fenix.tabstray.syncedtabs.OnTabClick as OnSyncedTabClick
import org.mozilla.fenix.tabstray.syncedtabs.OnTabCloseClick as OnSyncedTabClose

/**
 * Top-level UI for displaying the Tabs Tray feature.
 *
 * @param appStore [AppStore] used to listen for changes to [AppState].
 * @param browserStore [BrowserStore] used to listen for changes to [BrowserState].
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 * @param displayTabsInGrid Whether the normal and private tabs should be displayed in a grid.
 * @param isInDebugMode True for debug variant or if secret menu is enabled for this session.
 * @param shouldShowTabAutoCloseBanner Whether the tab auto closer banner should be displayed.
 * @param shouldShowInactiveTabsAutoCloseDialog Whether the inactive tabs auto close dialog should be displayed.
 * @param onTabPageClick Invoked when the user clicks on the Normal, Private, or Synced tabs page button.
 * @param onTabClose Invoked when the user clicks to close a tab.
 * @param onTabMediaClick Invoked when the user interacts with a tab's media controls.
 * @param onTabClick Invoked when the user clicks on a tab.
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
 * @param onSyncedTabClick Invoked when the user clicks on a synced tab.
 * @param onSyncedTabClose Invoked when the user clicks on a synced tab's close button.
 * @param onSaveToCollectionClick Invoked when the user clicks on the save to collection button from
 * the multi select banner.
 * @param onShareSelectedTabsClick Invoked when the user clicks on the share button from the
 * multi select banner.
 * @param onShareAllTabsClick Invoked when the user clicks on the share all tabs banner menu item.
 * @param onTabSettingsClick Invoked when the user clicks on the tab settings banner menu item.
 * @param onRecentlyClosedClick Invoked when the user clicks on the recently closed banner menu item.
 * @param onAccountSettingsClick Invoked when the user clicks on the account settings banner menu item.
 * @param onDeleteAllTabsClick Invoked when the user clicks on the close all tabs banner menu item.
 * @param onBookmarkSelectedTabsClick Invoked when the user clicks on the bookmark banner menu item.
 * @param onDeleteSelectedTabsClick Invoked when the user clicks on the close selected tabs banner menu item.
 * @param onForceSelectedTabsAsInactiveClick Invoked when the user clicks on the make inactive banner menu item.
 * @param onTabsTrayDismiss Invoked when accessibility services or UI automation requests dismissal.
 * @param onTabAutoCloseBannerViewOptionsClick Invoked when the user clicks to view the auto close options.
 * @param onTabAutoCloseBannerDismiss Invoked when the user clicks to dismiss the auto close banner.
 * @param onTabAutoCloseBannerShown Invoked when the auto close banner has been shown to the user.
 * @param onMove Invoked after the drag and drop gesture completed. Swaps positions of two tabs.
 * @param shouldShowInactiveTabsCFR Returns whether the inactive tabs CFR should be displayed.
 * @param onInactiveTabsCFRShown Invoked when the inactive tabs CFR is displayed.
 * @param onInactiveTabsCFRClick Invoked when the inactive tabs CFR is clicked.
 * @param onInactiveTabsCFRDismiss Invoked when the inactive tabs CFR is dismissed.
 */
@OptIn(ExperimentalFoundationApi::class)
@Suppress("LongMethod", "LongParameterList", "ComplexMethod")
@Composable
fun TabsTray(
    appStore: AppStore,
    browserStore: BrowserStore,
    tabsTrayStore: TabsTrayStore,
    displayTabsInGrid: Boolean,
    isInDebugMode: Boolean,
    shouldShowTabAutoCloseBanner: Boolean,
    shouldShowInactiveTabsAutoCloseDialog: (Int) -> Boolean,
    onTabPageClick: (Page) -> Unit,
    onTabClose: (TabSessionState) -> Unit,
    onTabMediaClick: (TabSessionState) -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabLongClick: (TabSessionState) -> Unit,
    onInactiveTabsHeaderClick: (Boolean) -> Unit,
    onDeleteAllInactiveTabsClick: () -> Unit,
    onInactiveTabsAutoCloseDialogShown: () -> Unit,
    onInactiveTabAutoCloseDialogCloseButtonClick: () -> Unit,
    onEnableInactiveTabAutoCloseClick: () -> Unit,
    onInactiveTabClick: (TabSessionState) -> Unit,
    onInactiveTabClose: (TabSessionState) -> Unit,
    onSyncedTabClick: OnSyncedTabClick,
    onSyncedTabClose: OnSyncedTabClose,
    onSaveToCollectionClick: () -> Unit,
    onShareSelectedTabsClick: () -> Unit,
    onShareAllTabsClick: () -> Unit,
    onTabSettingsClick: () -> Unit,
    onRecentlyClosedClick: () -> Unit,
    onAccountSettingsClick: () -> Unit,
    onDeleteAllTabsClick: () -> Unit,
    onBookmarkSelectedTabsClick: () -> Unit,
    onDeleteSelectedTabsClick: () -> Unit,
    onForceSelectedTabsAsInactiveClick: () -> Unit,
    onTabsTrayDismiss: () -> Unit,
    onTabAutoCloseBannerViewOptionsClick: () -> Unit,
    onTabAutoCloseBannerDismiss: () -> Unit,
    onTabAutoCloseBannerShown: () -> Unit,
    onMove: (String, String?, Boolean) -> Unit,
    shouldShowInactiveTabsCFR: () -> Boolean,
    onInactiveTabsCFRShown: () -> Unit,
    onInactiveTabsCFRClick: () -> Unit,
    onInactiveTabsCFRDismiss: () -> Unit,
) {
    val multiselectMode by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.mode,
    ) { state -> state.mode }
    val selectedPage by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.selectedPage,
    ) { state -> state.selectedPage }

    val pagerState =
        rememberPagerState(initialPage = selectedPage.ordinal, pageCount = { Page.values().size })
    val isInMultiSelectMode = multiselectMode is TabsTrayState.Mode.Select

    val shapeModifier = if (isInMultiSelectMode) {
        Modifier
    } else {
        Modifier.clip(RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp))
    }

    LaunchedEffect(selectedPage) {
        pagerState.animateScrollToPage(selectedPage.ordinal)
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .then(shapeModifier)
            .background(FirefoxTheme.colors.layer1)
            .testTag(TabsTrayTestTag.tabsTray),
    ) {
        Box(modifier = Modifier.nestedScroll(rememberNestedScrollInteropConnection())) {
            TabsTrayBanner(
                tabsTrayStore = tabsTrayStore,
                isInDebugMode = isInDebugMode,
                shouldShowTabAutoCloseBanner = shouldShowTabAutoCloseBanner,
                onTabPageIndicatorClicked = onTabPageClick,
                onSaveToCollectionClick = onSaveToCollectionClick,
                onShareSelectedTabsClick = onShareSelectedTabsClick,
                onShareAllTabsClick = onShareAllTabsClick,
                onTabSettingsClick = onTabSettingsClick,
                onRecentlyClosedClick = onRecentlyClosedClick,
                onAccountSettingsClick = onAccountSettingsClick,
                onDeleteAllTabsClick = onDeleteAllTabsClick,
                onBookmarkSelectedTabsClick = onBookmarkSelectedTabsClick,
                onDeleteSelectedTabsClick = onDeleteSelectedTabsClick,
                onForceSelectedTabsAsInactiveClick = onForceSelectedTabsAsInactiveClick,
                onDismissClick = onTabsTrayDismiss,
                onTabAutoCloseBannerViewOptionsClick = onTabAutoCloseBannerViewOptionsClick,
                onTabAutoCloseBannerDismiss = onTabAutoCloseBannerDismiss,
                onTabAutoCloseBannerShown = onTabAutoCloseBannerShown,
            )
        }

        Divider()

        Box(modifier = Modifier.fillMaxSize()) {
            HorizontalPager(
                modifier = Modifier.fillMaxSize(),
                state = pagerState,
                beyondBoundsPageCount = 2,
                userScrollEnabled = false,
            ) { position ->
                when (Page.positionToPage(position)) {
                    Page.NormalTabs -> {
                        NormalTabsPage(
                            appStore = appStore,
                            browserStore = browserStore,
                            tabsTrayStore = tabsTrayStore,
                            displayTabsInGrid = displayTabsInGrid,
                            onTabClose = onTabClose,
                            onTabMediaClick = onTabMediaClick,
                            onTabClick = onTabClick,
                            onTabLongClick = onTabLongClick,
                            shouldShowInactiveTabsAutoCloseDialog = shouldShowInactiveTabsAutoCloseDialog,
                            onInactiveTabsHeaderClick = onInactiveTabsHeaderClick,
                            onDeleteAllInactiveTabsClick = onDeleteAllInactiveTabsClick,
                            onInactiveTabsAutoCloseDialogShown = onInactiveTabsAutoCloseDialogShown,
                            onInactiveTabAutoCloseDialogCloseButtonClick = onInactiveTabAutoCloseDialogCloseButtonClick,
                            onEnableInactiveTabAutoCloseClick = onEnableInactiveTabAutoCloseClick,
                            onInactiveTabClick = onInactiveTabClick,
                            onInactiveTabClose = onInactiveTabClose,
                            onMove = onMove,
                            shouldShowInactiveTabsCFR = shouldShowInactiveTabsCFR,
                            onInactiveTabsCFRShown = onInactiveTabsCFRShown,
                            onInactiveTabsCFRClick = onInactiveTabsCFRClick,
                            onInactiveTabsCFRDismiss = onInactiveTabsCFRDismiss,
                        )
                    }

                    Page.PrivateTabs -> {
                        PrivateTabsPage(
                            browserStore = browserStore,
                            tabsTrayStore = tabsTrayStore,
                            displayTabsInGrid = displayTabsInGrid,
                            onTabClose = onTabClose,
                            onTabMediaClick = onTabMediaClick,
                            onTabClick = onTabClick,
                            onTabLongClick = onTabLongClick,
                            onMove = onMove,
                        )
                    }

                    Page.SyncedTabs -> {
                        SyncedTabsPage(
                            tabsTrayStore = tabsTrayStore,
                            onTabClick = onSyncedTabClick,
                            onTabClose = onSyncedTabClose,
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
    val tabs = generateFakeTabsList()
    TabsTrayPreviewRoot(
        displayTabsInGrid = false,
        selectedTabId = tabs[0].id,
        normalTabs = tabs,
        privateTabs = generateFakeTabsList(
            tabCount = 7,
            isPrivate = true,
        ),
        syncedTabs = generateFakeSyncedTabsList(),
    )
}

@Suppress("MagicNumber")
@LightDarkPreview
@Composable
private fun TabsTrayMultiSelectPreview() {
    val tabs = generateFakeTabsList()
    TabsTrayPreviewRoot(
        selectedTabId = tabs[0].id,
        mode = TabsTrayState.Mode.Select(tabs.take(4).toSet()),
        normalTabs = tabs,
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
        privateTabs = generateFakeTabsList(isPrivate = true),
    )
}

@LightDarkPreview
@Composable
private fun TabsTraySyncedTabsPreview() {
    TabsTrayPreviewRoot(
        selectedPage = Page.SyncedTabs,
        syncedTabs = generateFakeSyncedTabsList(deviceCount = 3),
    )
}

@LightDarkPreview
@Composable
private fun TabsTrayAutoCloseBannerPreview() {
    TabsTrayPreviewRoot(
        normalTabs = generateFakeTabsList(),
        showTabAutoCloseBanner = true,
    )
}

@Suppress("LongMethod")
@Composable
private fun TabsTrayPreviewRoot(
    displayTabsInGrid: Boolean = true,
    selectedPage: Page = Page.NormalTabs,
    selectedTabId: String? = null,
    mode: TabsTrayState.Mode = TabsTrayState.Mode.Normal,
    normalTabs: List<TabSessionState> = emptyList(),
    inactiveTabs: List<TabSessionState> = emptyList(),
    privateTabs: List<TabSessionState> = emptyList(),
    syncedTabs: List<SyncedTabsListItem> = emptyList(),
    inactiveTabsExpanded: Boolean = false,
    showInactiveTabsAutoCloseDialog: Boolean = false,
    showTabAutoCloseBanner: Boolean = false,
) {
    var selectedPageState by remember { mutableStateOf(selectedPage) }
    val normalTabsState = remember { normalTabs.toMutableStateList() }
    val inactiveTabsState = remember { inactiveTabs.toMutableStateList() }
    val privateTabsState = remember { privateTabs.toMutableStateList() }
    val syncedTabsState = remember { syncedTabs.toMutableStateList() }
    var inactiveTabsExpandedState by remember { mutableStateOf(inactiveTabsExpanded) }
    var showInactiveTabsAutoCloseDialogState by remember { mutableStateOf(showInactiveTabsAutoCloseDialog) }

    val appStore = AppStore(
        initialState = AppState(
            inactiveTabsExpanded = inactiveTabsExpandedState,
        ),
    )
    val browserStore = BrowserStore(
        initialState = BrowserState(
            tabs = normalTabs + privateTabs,
            selectedTabId = selectedTabId,
        ),
    )
    val tabsTrayStore = TabsTrayStore(
        initialState = TabsTrayState(
            selectedPage = selectedPageState,
            mode = mode,
            inactiveTabs = inactiveTabsState,
            normalTabs = normalTabsState,
            privateTabs = privateTabsState,
            syncedTabs = syncedTabsState,
        ),
    )

    FirefoxTheme {
        TabsTray(
            appStore = appStore,
            browserStore = browserStore,
            tabsTrayStore = tabsTrayStore,
            displayTabsInGrid = displayTabsInGrid,
            isInDebugMode = false,
            shouldShowInactiveTabsAutoCloseDialog = { true },
            shouldShowTabAutoCloseBanner = showTabAutoCloseBanner,
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
            onTabClick = { tab ->
                when (tabsTrayStore.state.mode) {
                    TabsTrayState.Mode.Normal -> {}
                    is TabsTrayState.Mode.Select -> {
                        if (tabsTrayStore.state.mode.selectedTabs.contains(tab)) {
                            tabsTrayStore.dispatch(TabsTrayAction.RemoveSelectTab(tab))
                        } else {
                            tabsTrayStore.dispatch(TabsTrayAction.AddSelectTab(tab))
                        }
                    }
                }
            },
            onTabLongClick = { tab ->
                tabsTrayStore.dispatch(TabsTrayAction.AddSelectTab(tab))
            },
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
            onSyncedTabClick = {},
            onSyncedTabClose = { _, _ -> },
            onSaveToCollectionClick = {},
            onShareSelectedTabsClick = {},
            onShareAllTabsClick = {},
            onTabSettingsClick = {},
            onRecentlyClosedClick = {},
            onAccountSettingsClick = {},
            onDeleteAllTabsClick = {},
            onDeleteSelectedTabsClick = {},
            onBookmarkSelectedTabsClick = {},
            onForceSelectedTabsAsInactiveClick = {},
            onTabsTrayDismiss = {},
            onTabAutoCloseBannerViewOptionsClick = {},
            onTabAutoCloseBannerDismiss = {},
            onTabAutoCloseBannerShown = {},
            onMove = { _, _, _ -> },
            shouldShowInactiveTabsCFR = { false },
            onInactiveTabsCFRShown = {},
            onInactiveTabsCFRClick = {},
            onInactiveTabsCFRDismiss = {},
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

private fun generateFakeSyncedTabsList(deviceCount: Int = 1): List<SyncedTabsListItem> =
    List(deviceCount) { index ->
        SyncedTabsListItem.DeviceSection(
            displayName = "Device $index",
            tabs = listOf(
                generateFakeSyncedTab("Mozilla", "www.mozilla.org"),
                generateFakeSyncedTab("Google", "www.google.com"),
                generateFakeSyncedTab("", "www.google.com"),
            ),
        )
    }

private fun generateFakeSyncedTab(
    tabName: String,
    tabUrl: String,
    action: SyncedTabsListItem.Tab.Action = SyncedTabsListItem.Tab.Action.None,
): SyncedTabsListItem.Tab =
    SyncedTabsListItem.Tab(
        tabName.ifEmpty { tabUrl },
        tabUrl,
        action,
        SyncTab(
            history = listOf(TabEntry(tabName, tabUrl, null)),
            active = 0,
            lastUsed = 0L,
            inactive = false,
        ),
    )
