/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.Divider
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.LocalContentColor
import androidx.compose.material.Tab
import androidx.compose.material.TabRow
import androidx.compose.material.Text
import androidx.compose.material.ripple.LocalRippleTheme
import androidx.compose.material.ripple.RippleAlpha
import androidx.compose.material.ripple.RippleTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.ui.tabcounter.TabCounter
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ContextualMenu
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

private val ICON_SIZE = 24.dp

/**
 * Top-level UI for displaying the banner in [TabsTray].
 *
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 * @param isInDebugMode True for debug variant or if secret menu is enabled for this session.
 * @param onTabPageIndicatorClicked Invoked when the user clicks on a tab page indicator.
 * @param onSaveToCollectionClick Invoked when the user clicks on the save to collection button from
 * the multi select banner.
 * @param onShareSelectedTabsClick Invoked when the user clicks on the share button from the multi select banner.
 * @param onEnterMultiselectModeClick Invoked when the user clicks on the enter multiselect mode menu item.
 * @param onShareAllTabsClick Invoked when the user clicks on the share menu item.
 * @param onTabSettingsClick Invoked when the user clicks on the tab settings menu item.
 * @param onRecentlyClosedClick Invoked when the user clicks on the recently closed tabs menu item.
 * @param onAccountSettingsClick Invoked when the user clicks on the account settings menu item.
 * @param onDeleteAllTabsClick Invoked when user interacts with the close all tabs menu item.
 * @param onDeleteSelectedTabsClick Invoked when user interacts with the close menu item.
 * @param onBookmarkSelectedTabsClick Invoked when user interacts with the bookmark menu item.
 * @param onForceSelectedTabsAsInactiveClick Invoked when user interacts with the make inactive menu item.
 */
@Suppress("LongParameterList")
@Composable
fun TabsTrayBanner(
    tabsTrayStore: TabsTrayStore,
    isInDebugMode: Boolean,
    onTabPageIndicatorClicked: (Page) -> Unit,
    onSaveToCollectionClick: () -> Unit,
    onShareSelectedTabsClick: () -> Unit,
    onEnterMultiselectModeClick: () -> Unit,
    onShareAllTabsClick: () -> Unit,
    onTabSettingsClick: () -> Unit,
    onRecentlyClosedClick: () -> Unit,
    onAccountSettingsClick: () -> Unit,
    onDeleteAllTabsClick: () -> Unit,
    onDeleteSelectedTabsClick: () -> Unit,
    onBookmarkSelectedTabsClick: () -> Unit,
    onForceSelectedTabsAsInactiveClick: () -> Unit,
) {
    val normalTabCount = tabsTrayStore.observeAsComposableState { state ->
        state.normalTabs.size + state.inactiveTabs.size
    }.value ?: 0
    val privateTabCount = tabsTrayStore
        .observeAsComposableState { state -> state.privateTabs.size }.value ?: 0
    val multiselectMode = tabsTrayStore
        .observeAsComposableState { state -> state.mode }.value ?: TabsTrayState.Mode.Normal
    val selectedPage = tabsTrayStore
        .observeAsComposableState { state -> state.selectedPage }.value ?: Page.NormalTabs

    if (multiselectMode is TabsTrayState.Mode.Select) {
        MultiSelectBanner(
            selectedTabCount = multiselectMode.selectedTabs.size,
            shouldShowInactiveButton = isInDebugMode,
            onExitSelectModeClick = { tabsTrayStore.dispatch(TabsTrayAction.ExitSelectMode) },
            onSaveToCollectionsClick = onSaveToCollectionClick,
            onShareSelectedTabs = onShareSelectedTabsClick,
            onBookmarkSelectedTabsClick = onBookmarkSelectedTabsClick,
            onCloseSelectedTabsClick = onDeleteSelectedTabsClick,
            onMakeSelectedTabsInactive = onForceSelectedTabsAsInactiveClick,
        )
    } else {
        SingleSelectBanner(
            onTabPageIndicatorClicked = onTabPageIndicatorClicked,
            selectedPage = selectedPage,
            normalTabCount = normalTabCount,
            privateTabCount = privateTabCount,
            onEnterMultiselectModeClick = onEnterMultiselectModeClick,
            onShareAllTabsClick = onShareAllTabsClick,
            onTabSettingsClick = onTabSettingsClick,
            onRecentlyClosedClick = onRecentlyClosedClick,
            onAccountSettingsClick = onAccountSettingsClick,
            onDeleteAllTabsClick = onDeleteAllTabsClick,
        )
    }
}

@Suppress("LongMethod", "LongParameterList")
@Composable
private fun SingleSelectBanner(
    selectedPage: Page,
    normalTabCount: Int,
    privateTabCount: Int,
    onTabPageIndicatorClicked: (Page) -> Unit,
    onEnterMultiselectModeClick: () -> Unit,
    onShareAllTabsClick: () -> Unit,
    onTabSettingsClick: () -> Unit,
    onRecentlyClosedClick: () -> Unit,
    onAccountSettingsClick: () -> Unit,
    onDeleteAllTabsClick: () -> Unit,
) {
    val selectedColor = FirefoxTheme.colors.iconActive
    val inactiveColor = FirefoxTheme.colors.iconPrimaryInactive
    var showMenu by remember { mutableStateOf(false) }

    Column {
        Spacer(modifier = Modifier.height(dimensionResource(id = R.dimen.bottom_sheet_handle_top_margin)))

        Divider(
            modifier = Modifier
                .fillMaxWidth(DRAG_INDICATOR_WIDTH_PERCENT)
                .align(Alignment.CenterHorizontally),
            color = FirefoxTheme.colors.textSecondary,
            thickness = dimensionResource(id = R.dimen.bottom_sheet_handle_height),
        )

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(80.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            CompositionLocalProvider(LocalRippleTheme provides DisabledRippleTheme) {
                TabRow(
                    selectedTabIndex = selectedPage.ordinal,
                    modifier = Modifier.width(180.dp),
                    backgroundColor = Color.Transparent,
                    contentColor = selectedColor,
                    divider = {},
                ) {
                    Tab(
                        selected = selectedPage == Page.NormalTabs,
                        onClick = { onTabPageIndicatorClicked(Page.NormalTabs) },
                        modifier = Modifier
                            .fillMaxHeight()
                            .testTag(TabsTrayTestTag.normalTabsPageButton),
                        selectedContentColor = selectedColor,
                        unselectedContentColor = inactiveColor,
                    ) {
                        NormalTabsTabIcon(normalTabCount = normalTabCount)
                    }

                    Tab(
                        selected = selectedPage == Page.PrivateTabs,
                        onClick = { onTabPageIndicatorClicked(Page.PrivateTabs) },
                        modifier = Modifier
                            .fillMaxHeight()
                            .testTag(TabsTrayTestTag.privateTabsPageButton),
                        icon = {
                            Icon(
                                painter = painterResource(id = R.drawable.ic_private_browsing),
                                contentDescription = stringResource(id = R.string.tabs_header_private_tabs_title),
                            )
                        },
                        selectedContentColor = selectedColor,
                        unselectedContentColor = inactiveColor,
                    )

                    Tab(
                        selected = selectedPage == Page.SyncedTabs,
                        onClick = { onTabPageIndicatorClicked(Page.SyncedTabs) },
                        modifier = Modifier
                            .fillMaxHeight()
                            .testTag(TabsTrayTestTag.syncedTabsPageButton),
                        icon = {
                            Icon(
                                painter = painterResource(id = R.drawable.ic_synced_tabs),
                                contentDescription = stringResource(id = R.string.tabs_header_synced_tabs_title),
                            )
                        },
                        selectedContentColor = selectedColor,
                        unselectedContentColor = inactiveColor,
                    )
                }
            }

            Spacer(modifier = Modifier.weight(1.0f))

            IconButton(
                onClick = { showMenu = true },
                modifier = Modifier
                    .align(Alignment.CenterVertically)
                    .testTag(TabsTrayTestTag.threeDotButton),
            ) {
                ContextualMenu(
                    menuItems = generateSingleSelectBannerMenuItems(
                        selectedPage,
                        normalTabCount,
                        privateTabCount,
                        onTabSettingsClick,
                        onRecentlyClosedClick,
                        onEnterMultiselectModeClick,
                        onShareAllTabsClick,
                        onDeleteAllTabsClick,
                        onAccountSettingsClick,
                    ),
                    showMenu = showMenu,
                    offset = DpOffset(x = 0.dp, y = -ICON_SIZE),
                    onDismissRequest = { showMenu = false },
                )
                Icon(
                    painter = painterResource(R.drawable.ic_menu),
                    contentDescription = stringResource(id = R.string.open_tabs_menu),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }
    }
}

@Suppress("LongParameterList")
@Composable
private fun generateSingleSelectBannerMenuItems(
    selectedPage: Page,
    normalTabCount: Int,
    privateTabCount: Int,
    onTabSettingsClick: () -> Unit,
    onRecentlyClosedClick: () -> Unit,
    onEnterMultiselectModeClick: () -> Unit,
    onShareAllTabsClick: () -> Unit,
    onDeleteAllTabsClick: () -> Unit,
    onAccountSettingsClick: () -> Unit,
): List<MenuItem> {
    val tabSettingsItem = MenuItem(
        title = stringResource(id = R.string.tab_tray_menu_tab_settings),
        testTag = TabsTrayTestTag.tabSettings,
        onClick = onTabSettingsClick,
    )
    val recentlyClosedTabsItem = MenuItem(
        title = stringResource(id = R.string.tab_tray_menu_recently_closed),
        testTag = TabsTrayTestTag.recentlyClosedTabs,
        onClick = onRecentlyClosedClick,
    )
    val enterSelectModeItem = MenuItem(
        title = stringResource(id = R.string.tabs_tray_select_tabs),
        testTag = TabsTrayTestTag.selectTabs,
        onClick = onEnterMultiselectModeClick,
    )
    val shareAllTabsItem = MenuItem(
        title = stringResource(id = R.string.tab_tray_menu_item_share),
        testTag = TabsTrayTestTag.shareAllTabs,
        onClick = onShareAllTabsClick,
    )
    val deleteAllTabsItem = MenuItem(
        title = stringResource(id = R.string.tab_tray_menu_item_close),
        testTag = TabsTrayTestTag.closeAllTabs,
        onClick = onDeleteAllTabsClick,
    )
    val accountSettingsItem = MenuItem(
        title = stringResource(id = R.string.tab_tray_menu_account_settings),
        testTag = TabsTrayTestTag.accountSettings,
        onClick = onAccountSettingsClick,
    )
    return when {
        selectedPage == Page.NormalTabs && normalTabCount == 0 ||
            selectedPage == Page.PrivateTabs && privateTabCount == 0 -> listOf(
            tabSettingsItem,
            recentlyClosedTabsItem,
        )

        selectedPage == Page.NormalTabs -> listOf(
            enterSelectModeItem,
            shareAllTabsItem,
            tabSettingsItem,
            recentlyClosedTabsItem,
            deleteAllTabsItem,
        )

        selectedPage == Page.PrivateTabs -> listOf(
            tabSettingsItem,
            recentlyClosedTabsItem,
            deleteAllTabsItem,
        )

        selectedPage == Page.SyncedTabs -> listOf(
            accountSettingsItem,
            recentlyClosedTabsItem,
        )

        else -> emptyList()
    }
}

@Composable
private fun NormalTabsTabIcon(normalTabCount: Int) {
    val normalTabCountText: String
    val normalTabCountTextModifier: Modifier
    if (normalTabCount > TabCounter.MAX_VISIBLE_TABS) {
        normalTabCountText = TabCounter.SO_MANY_TABS_OPEN
        normalTabCountTextModifier = Modifier.padding(bottom = 1.dp)
    } else {
        normalTabCountText = normalTabCount.toString()
        normalTabCountTextModifier = Modifier
    }
    val normalTabsContentDescription = if (normalTabCount == 1) {
        stringResource(id = R.string.mozac_tab_counter_open_tab_tray_single)
    } else {
        stringResource(id = R.string.mozac_tab_counter_open_tab_tray_plural, normalTabCount.toString())
    }

    Box(
        modifier = Modifier
            .semantics(mergeDescendants = true) {
                testTag = TabsTrayTestTag.normalTabsCounter
            },
    ) {
        Icon(
            painter = painterResource(
                id = mozilla.components.ui.tabcounter.R.drawable.mozac_ui_tabcounter_box,
            ),
            contentDescription = normalTabsContentDescription,
            modifier = Modifier.align(Alignment.Center),
        )

        Text(
            text = normalTabCountText,
            modifier = normalTabCountTextModifier.align(Alignment.Center),
            color = LocalContentColor.current,
            fontSize = with(LocalDensity.current) { 12.dp.toSp() },
            fontWeight = FontWeight.W700,
        )
    }
}

/**
 * Banner displayed in multi select mode.
 *
 * @param selectedTabCount Number of selected tabs.
 * @param shouldShowInactiveButton Whether or not to show the inactive tabs menu item.
 * @param onExitSelectModeClick Invoked when the user clicks on exit select mode button.
 * @param onSaveToCollectionsClick Invoked when the user clicks on the save to collection button.
 * @param onShareSelectedTabs Invoked when the user clicks on the share button.
 * @param onBookmarkSelectedTabsClick Invoked when user interacts with the bookmark menu item.
 * @param onCloseSelectedTabsClick Invoked when user interacts with the close menu item.
 * @param onMakeSelectedTabsInactive Invoked when user interacts with the make inactive menu item.
 */
@Suppress("LongMethod", "LongParameterList")
@Composable
private fun MultiSelectBanner(
    selectedTabCount: Int,
    shouldShowInactiveButton: Boolean,
    onExitSelectModeClick: () -> Unit,
    onSaveToCollectionsClick: () -> Unit,
    onShareSelectedTabs: () -> Unit,
    onBookmarkSelectedTabsClick: () -> Unit,
    onCloseSelectedTabsClick: () -> Unit,
    onMakeSelectedTabsInactive: () -> Unit,
) {
    var showMenu by remember { mutableStateOf(false) }
    val menuItems = mutableListOf(
        MenuItem(
            title = stringResource(R.string.tab_tray_multiselect_menu_item_bookmark),
            onClick = onBookmarkSelectedTabsClick,
        ),
        MenuItem(
            title = stringResource(R.string.tab_tray_multiselect_menu_item_close),
            onClick = onCloseSelectedTabsClick,
        ),
    )
    if (shouldShowInactiveButton) {
        menuItems.add(
            MenuItem(
                title = stringResource(R.string.inactive_tabs_menu_item),
                onClick = onMakeSelectedTabsInactive,
            ),
        )
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(88.dp)
            .background(color = FirefoxTheme.colors.layerAccent),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        IconButton(onClick = onExitSelectModeClick) {
            Icon(
                painter = painterResource(id = R.drawable.ic_close),
                contentDescription = stringResource(id = R.string.tab_tray_close_multiselect_content_description),
                tint = FirefoxTheme.colors.iconOnColor,
            )
        }

        Text(
            text = stringResource(R.string.tab_tray_multi_select_title, selectedTabCount),
            style = FirefoxTheme.typography.body1,
            color = FirefoxTheme.colors.textOnColorPrimary,
            fontSize = 20.sp,
            fontWeight = FontWeight.W500,
        )

        Spacer(modifier = Modifier.weight(1.0f))

        IconButton(onClick = onSaveToCollectionsClick) {
            Icon(
                painter = painterResource(id = R.drawable.ic_tab_collection),
                contentDescription = stringResource(
                    id = R.string.tab_tray_collection_button_multiselect_content_description,
                ),
                tint = FirefoxTheme.colors.iconOnColor,
            )
        }

        IconButton(onClick = onShareSelectedTabs) {
            Icon(
                painter = painterResource(id = R.drawable.ic_share),
                contentDescription = stringResource(
                    id = R.string.tab_tray_multiselect_share_content_description,
                ),
                tint = FirefoxTheme.colors.iconOnColor,
            )
        }

        IconButton(onClick = { showMenu = true }) {
            Icon(
                painter = painterResource(id = R.drawable.ic_menu),
                contentDescription = stringResource(id = R.string.tab_tray_multiselect_menu_content_description),
                tint = FirefoxTheme.colors.iconOnColor,
            )

            ContextualMenu(
                menuItems = menuItems,
                showMenu = showMenu,
                offset = DpOffset(x = 0.dp, y = -ICON_SIZE),
                onDismissRequest = { showMenu = false },
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayBannerPreview() {
    TabsTrayBannerPreviewRoot(
        selectedPage = Page.PrivateTabs,
        normalTabCount = 5,
    )
}

@LightDarkPreview
@Composable
private fun TabsTrayBannerInfinityPreview() {
    TabsTrayBannerPreviewRoot(
        normalTabCount = TabCounter.MAX_VISIBLE_TABS + 1,
    )
}

@LightDarkPreview
@Composable
private fun TabsTrayBannerMultiselectPreview() {
    TabsTrayBannerPreviewRoot(
        selectMode = TabsTrayState.Mode.Select(
            setOf(
                TabSessionState(
                    id = "1",
                    content = ContentState(
                        url = "www.mozilla.com",
                    ),
                ),
                TabSessionState(
                    id = "2",
                    content = ContentState(
                        url = "www.mozilla.com",
                    ),
                ),
            ),
        ),
    )
}

@Composable
private fun TabsTrayBannerPreviewRoot(
    selectMode: TabsTrayState.Mode = TabsTrayState.Mode.Normal,
    selectedPage: Page = Page.NormalTabs,
    normalTabCount: Int = 10,
    privateTabCount: Int = 10,
) {
    val normalTabs = generateFakeTabsList(normalTabCount)
    val privateTabs = generateFakeTabsList(privateTabCount)

    val tabsTrayStore = TabsTrayStore(
        initialState = TabsTrayState(
            selectedPage = selectedPage,
            mode = selectMode,
            normalTabs = normalTabs,
            privateTabs = privateTabs,
        ),
    )

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            TabsTrayBanner(
                tabsTrayStore = tabsTrayStore,
                isInDebugMode = true,
                onTabPageIndicatorClicked = { page ->
                    tabsTrayStore.dispatch(TabsTrayAction.PageSelected(page))
                },
                onSaveToCollectionClick = {},
                onShareSelectedTabsClick = {},
                onEnterMultiselectModeClick = {},
                onShareAllTabsClick = {},
                onTabSettingsClick = {},
                onRecentlyClosedClick = {},
                onAccountSettingsClick = {},
                onDeleteAllTabsClick = {},
                onBookmarkSelectedTabsClick = {},
                onDeleteSelectedTabsClick = {},
                onForceSelectedTabsAsInactiveClick = {},
            )
        }
    }
}

private const val DRAG_INDICATOR_WIDTH_PERCENT = 0.1f

private object DisabledRippleTheme : RippleTheme {
    @Composable
    override fun defaultColor() = Color.Unspecified

    @Composable
    override fun rippleAlpha(): RippleAlpha = RippleAlpha(0.0f, 0.0f, 0.0f, 0.0f)
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
