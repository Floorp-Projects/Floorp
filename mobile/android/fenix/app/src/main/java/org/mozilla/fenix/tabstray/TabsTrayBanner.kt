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
import androidx.compose.foundation.layout.size
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.LocalContentAlpha
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.ui.tabcounter.TabCounter
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Banner
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.compose.ContextualMenu
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.tabstray.ext.getMenuItems
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.max

private val ICON_SIZE = 24.dp
private const val MAX_WIDTH_TAB_ROW_PERCENT = 0.5f
private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f
private const val TAB_COUNT_SHOW_CFR = 6

/**
 * Top-level UI for displaying the banner in [TabsTray].
 *
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 * @param isInDebugMode True for debug variant or if secret menu is enabled for this session.
 * @param shouldShowTabAutoCloseBanner Whether the tab auto closer banner should be displayed.
 * @param onTabPageIndicatorClicked Invoked when the user clicks on a tab page indicator.
 * @param onSaveToCollectionClick Invoked when the user clicks on the save to collection button from
 * the multi select banner.
 * @param onShareSelectedTabsClick Invoked when the user clicks on the share button from the multi select banner.
 * @param onShareAllTabsClick Invoked when the user clicks on the share menu item.
 * @param onTabSettingsClick Invoked when the user clicks on the tab settings menu item.
 * @param onRecentlyClosedClick Invoked when the user clicks on the recently closed tabs menu item.
 * @param onAccountSettingsClick Invoked when the user clicks on the account settings menu item.
 * @param onDeleteAllTabsClick Invoked when user interacts with the close all tabs menu item.
 * @param onDeleteSelectedTabsClick Invoked when user interacts with the close menu item.
 * @param onBookmarkSelectedTabsClick Invoked when user interacts with the bookmark menu item.
 * @param onForceSelectedTabsAsInactiveClick Invoked when user interacts with the make inactive menu item.
 * @param onDismissClick Invoked when accessibility services or UI automation requests dismissal.
 * @param onTabAutoCloseBannerViewOptionsClick Invoked when the user clicks to view the auto close options.
 * @param onTabAutoCloseBannerDismiss Invoked when the user clicks to dismiss the auto close banner.
 * @param onTabAutoCloseBannerShown Invoked when the auto close banner has been shown to the user.
 */
@Suppress("LongParameterList")
@Composable
fun TabsTrayBanner(
    tabsTrayStore: TabsTrayStore,
    isInDebugMode: Boolean,
    shouldShowTabAutoCloseBanner: Boolean,
    onTabPageIndicatorClicked: (Page) -> Unit,
    onSaveToCollectionClick: () -> Unit,
    onShareSelectedTabsClick: () -> Unit,
    onShareAllTabsClick: () -> Unit,
    onTabSettingsClick: () -> Unit,
    onRecentlyClosedClick: () -> Unit,
    onAccountSettingsClick: () -> Unit,
    onDeleteAllTabsClick: () -> Unit,
    onDeleteSelectedTabsClick: () -> Unit,
    onBookmarkSelectedTabsClick: () -> Unit,
    onForceSelectedTabsAsInactiveClick: () -> Unit,
    onDismissClick: () -> Unit,
    onTabAutoCloseBannerViewOptionsClick: () -> Unit,
    onTabAutoCloseBannerDismiss: () -> Unit,
    onTabAutoCloseBannerShown: () -> Unit,
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
    val showTabAutoCloseBanner = tabsTrayStore.observeAsComposableState { state ->
        shouldShowTabAutoCloseBanner && max(state.normalTabs.size, state.privateTabs.size) >= TAB_COUNT_SHOW_CFR
    }.value ?: false
    var hasAcknowledgedBanner by remember { mutableStateOf(false) }

    val menuItems = multiselectMode.getMenuItems(
        resources = LocalContext.current.resources,
        shouldShowInactiveButton = isInDebugMode,
        onBookmarkSelectedTabsClick = onBookmarkSelectedTabsClick,
        onCloseSelectedTabsClick = onDeleteSelectedTabsClick,
        onMakeSelectedTabsInactive = onForceSelectedTabsAsInactiveClick,

        selectedPage = selectedPage,
        normalTabCount = normalTabCount,
        privateTabCount = privateTabCount,
        onTabSettingsClick = onTabSettingsClick,
        onRecentlyClosedClick = onRecentlyClosedClick,
        onEnterMultiselectModeClick = { tabsTrayStore.dispatch(TabsTrayAction.EnterSelectMode) },
        onShareAllTabsClick = onShareAllTabsClick,
        onDeleteAllTabsClick = onDeleteAllTabsClick,
        onAccountSettingsClick = onAccountSettingsClick,
    )

    Column {
        if (multiselectMode is TabsTrayState.Mode.Select) {
            MultiSelectBanner(
                menuItems = menuItems,
                selectedTabCount = multiselectMode.selectedTabs.size,
                onExitSelectModeClick = { tabsTrayStore.dispatch(TabsTrayAction.ExitSelectMode) },
                onSaveToCollectionsClick = onSaveToCollectionClick,
                onShareSelectedTabs = onShareSelectedTabsClick,
            )
        } else {
            SingleSelectBanner(
                menuItems = menuItems,
                selectedPage = selectedPage,
                normalTabCount = normalTabCount,
                onTabPageIndicatorClicked = onTabPageIndicatorClicked,
                onDismissClick = onDismissClick,
            )
        }

        if (!hasAcknowledgedBanner && showTabAutoCloseBanner) {
            onTabAutoCloseBannerShown()

            Divider()

            Banner(
                message = stringResource(id = R.string.tab_tray_close_tabs_banner_message),
                button1Text = stringResource(id = R.string.tab_tray_close_tabs_banner_negative_button_text),
                button2Text = stringResource(id = R.string.tab_tray_close_tabs_banner_positive_button_text),
                onButton1Click = {
                    hasAcknowledgedBanner = true
                    onTabAutoCloseBannerViewOptionsClick()
                },
                onButton2Click = {
                    hasAcknowledgedBanner = true
                    onTabAutoCloseBannerDismiss()
                },
            )
        }
    }
}

@Suppress("LongMethod")
@Composable
private fun SingleSelectBanner(
    menuItems: List<MenuItem>,
    selectedPage: Page,
    normalTabCount: Int,
    onTabPageIndicatorClicked: (Page) -> Unit,
    onDismissClick: () -> Unit,
) {
    val selectedColor = FirefoxTheme.colors.iconActive
    val inactiveColor = FirefoxTheme.colors.iconPrimaryInactive
    var showMenu by remember { mutableStateOf(false) }

    Column(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
        Spacer(modifier = Modifier.height(dimensionResource(id = R.dimen.bottom_sheet_handle_top_margin)))

        BottomSheetHandle(
            onRequestDismiss = onDismissClick,
            contentDescription = stringResource(R.string.a11y_action_label_collapse),
            modifier = Modifier
                .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                .align(Alignment.CenterHorizontally)
                .testTag(TabsTrayTestTag.bannerHandle),
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
                    modifier = Modifier.fillMaxWidth(MAX_WIDTH_TAB_ROW_PERCENT),
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
                    menuItems = menuItems,
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

private const val MAX_VISIBLE_TABS = 99
private const val SO_MANY_TABS_OPEN = "∞"
private val NORMAL_TABS_BOTTOM_PADDING = 0.5.dp
private const val ONE_DIGIT_SIZE_RATIO = 0.5f
private const val TWO_DIGITS_SIZE_RATIO = 0.4f

@Composable
@Suppress("MagicNumber")
private fun NormalTabsTabIcon(normalTabCount: Int) {
    val normalTabCountText: String
    val tabCountTextRatio: Float
    val needsBottomPaddingForInfiniteTabs: Boolean

    when (normalTabCount) {
        in 0..9 -> {
            normalTabCountText = normalTabCount.toString()
            tabCountTextRatio = ONE_DIGIT_SIZE_RATIO
            needsBottomPaddingForInfiniteTabs = false
        }

        in 10..MAX_VISIBLE_TABS -> {
            normalTabCountText = normalTabCount.toString()
            tabCountTextRatio = TWO_DIGITS_SIZE_RATIO
            needsBottomPaddingForInfiniteTabs = false
        }

        else -> {
            normalTabCountText = SO_MANY_TABS_OPEN
            tabCountTextRatio = ONE_DIGIT_SIZE_RATIO
            needsBottomPaddingForInfiniteTabs = true
        }
    }

    val normalTabsContentDescription = if (normalTabCount == 1) {
        stringResource(id = R.string.mozac_tab_counter_open_tab_tray_single)
    } else {
        stringResource(
            id = R.string.mozac_tab_counter_open_tab_tray_plural,
            normalTabCount.toString(),
        )
    }

    val counterBoxWidthDp =
        dimensionResource(id = mozilla.components.ui.tabcounter.R.dimen.mozac_tab_counter_box_width_height)
    val counterBoxWidthPx = LocalDensity.current.run { counterBoxWidthDp.roundToPx() }
    val counterTabsTextSize = (tabCountTextRatio * counterBoxWidthPx).toInt()

    val normalTabsTextModifier = if (needsBottomPaddingForInfiniteTabs) {
        val bottomPadding = with(LocalDensity.current) { counterTabsTextSize.toDp() / 4 }
        Modifier.padding(bottom = bottomPadding)
    } else {
        Modifier.padding(bottom = NORMAL_TABS_BOTTOM_PADDING)
    }

    Box(
        modifier = Modifier
            .semantics(mergeDescendants = true) {
                testTag = TabsTrayTestTag.normalTabsCounter
            },
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            painter = painterResource(
                id = mozilla.components.ui.tabcounter.R.drawable.mozac_ui_tabcounter_box,
            ),
            contentDescription = normalTabsContentDescription,
        )

        Text(
            text = normalTabCountText,
            modifier = normalTabsTextModifier,
            color = LocalContentColor.current.copy(alpha = LocalContentAlpha.current),
            fontSize = with(LocalDensity.current) { counterTabsTextSize.toDp().toSp() },
            fontWeight = FontWeight.W700,
            textAlign = TextAlign.Center,
        )
    }
}

/**
 * Banner displayed in multi select mode.
 *
 * @param menuItems List of items in the menu.
 * @param selectedTabCount Number of selected tabs.
 * @param onExitSelectModeClick Invoked when the user clicks on exit select mode button.
 * @param onSaveToCollectionsClick Invoked when the user clicks on the save to collection button.
 * @param onShareSelectedTabs Invoked when the user clicks on the share button.
 */
@Suppress("LongMethod")
@Composable
private fun MultiSelectBanner(
    menuItems: List<MenuItem>,
    selectedTabCount: Int,
    onExitSelectModeClick: () -> Unit,
    onSaveToCollectionsClick: () -> Unit,
    onShareSelectedTabs: () -> Unit,
) {
    var showMenu by remember { mutableStateOf(false) }

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
            modifier = Modifier.testTag(TabsTrayTestTag.selectionCounter),
            style = FirefoxTheme.typography.headline6,
            color = FirefoxTheme.colors.textOnColorPrimary,
        )

        Spacer(modifier = Modifier.weight(1.0f))

        IconButton(
            onClick = onSaveToCollectionsClick,
            modifier = Modifier.testTag(TabsTrayTestTag.collectionsButton),
            enabled = selectedTabCount > 0,
        ) {
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
private fun TabsTrayBannerAutoClosePreview() {
    TabsTrayBannerPreviewRoot(
        shouldShowTabAutoCloseBanner = true,
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
    shouldShowTabAutoCloseBanner: Boolean = false,
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
        Box(modifier = Modifier.size(400.dp)) {
            TabsTrayBanner(
                tabsTrayStore = tabsTrayStore,
                isInDebugMode = true,
                shouldShowTabAutoCloseBanner = shouldShowTabAutoCloseBanner,
                onTabPageIndicatorClicked = { page ->
                    tabsTrayStore.dispatch(TabsTrayAction.PageSelected(page))
                },
                onSaveToCollectionClick = {},
                onShareSelectedTabsClick = {},
                onShareAllTabsClick = {},
                onTabSettingsClick = {},
                onRecentlyClosedClick = {},
                onAccountSettingsClick = {},
                onDeleteAllTabsClick = {},
                onBookmarkSelectedTabsClick = {},
                onDeleteSelectedTabsClick = {},
                onForceSelectedTabsAsInactiveClick = {},
                onDismissClick = {},
                onTabAutoCloseBannerViewOptionsClick = {},
                onTabAutoCloseBannerDismiss = {},
                onTabAutoCloseBannerShown = {},
            )
        }
    }
}

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
