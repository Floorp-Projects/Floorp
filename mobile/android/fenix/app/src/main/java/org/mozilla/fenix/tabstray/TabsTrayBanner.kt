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
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.ui.tabcounter.TabCounter
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.DropdownMenu
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

private val ICON_SIZE = 24.dp

/**
 * Top-level UI for displaying the banner in [TabsTray].
 *
 * @param selectMode Current [TabsTrayState.Mode] used in the tabs tray.
 * @param selectedPage The active [Page] of the Tabs Tray.
 * @param normalTabCount The total amount of normal browsing tabs currently open.
 * @param isInDebugMode True for debug variant or if secret menu is enabled for this session.
 * @param onTabPageIndicatorClicked Invoked when the user clicks on a tab page indicator.
 * @param onExitSelectModeClick Invoked when the user clicks on exit select mode button from the
 * multi select banner.
 */
@Suppress("LongParameterList")
@Composable
fun TabsTrayBanner(
    selectMode: TabsTrayState.Mode,
    selectedPage: Page,
    normalTabCount: Int,
    isInDebugMode: Boolean,
    onTabPageIndicatorClicked: (Page) -> Unit,
    onExitSelectModeClick: () -> Unit,
) {
    if (selectMode is TabsTrayState.Mode.Select) {
        MultiSelectBanner(
            selectedTabCount = selectMode.selectedTabs.size,
            shouldShowInactiveButton = isInDebugMode,
            onExitSelectModeClick = onExitSelectModeClick,
        )
    } else {
        SingleSelectBanner(
            onTabPageIndicatorClicked = onTabPageIndicatorClicked,
            selectedPage = selectedPage,
            normalTabCount = normalTabCount,
        )
    }
}

@Suppress("LongMethod")
@Composable
private fun SingleSelectBanner(
    selectedPage: Page,
    normalTabCount: Int,
    onTabPageIndicatorClicked: (Page) -> Unit,
) {
    val selectedColor = FirefoxTheme.colors.iconActive
    val inactiveColor = FirefoxTheme.colors.iconPrimaryInactive

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
                        modifier = Modifier.fillMaxHeight(),
                        selectedContentColor = selectedColor,
                        unselectedContentColor = inactiveColor,
                    ) {
                        NormalTabsTabIcon(normalTabCount = normalTabCount)
                    }

                    Tab(
                        selected = selectedPage == Page.PrivateTabs,
                        onClick = { onTabPageIndicatorClicked(Page.PrivateTabs) },
                        modifier = Modifier.fillMaxHeight(),
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
                        modifier = Modifier.fillMaxHeight(),
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
                onClick = {},
                modifier = Modifier.align(Alignment.CenterVertically),
            ) {
                Icon(
                    painter = painterResource(R.drawable.ic_menu),
                    contentDescription = stringResource(id = R.string.open_tabs_menu),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }
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

    Box {
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
 */
@Suppress("LongMethod")
@Composable
private fun MultiSelectBanner(
    selectedTabCount: Int,
    shouldShowInactiveButton: Boolean,
    onExitSelectModeClick: () -> Unit,
) {
    var showMenu by remember { mutableStateOf(false) }
    val menuItems = mutableListOf(
        MenuItem(
            title = stringResource(R.string.tab_tray_multiselect_menu_item_bookmark),
        ) {},
        MenuItem(
            title = stringResource(R.string.tab_tray_multiselect_menu_item_close),
        ) {},
    )
    if (shouldShowInactiveButton) {
        menuItems.add(
            MenuItem(
                title = stringResource(R.string.inactive_tabs_menu_item),
            ) {},
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

        IconButton(onClick = {}) {
            Icon(
                painter = painterResource(id = R.drawable.ic_tab_collection),
                contentDescription = stringResource(
                    id = R.string.tab_tray_collection_button_multiselect_content_description,
                ),
                tint = FirefoxTheme.colors.iconOnColor,
            )
        }

        IconButton(onClick = {}) {
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

            DropdownMenu(
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
) {
    var selectedPageState by remember { mutableStateOf(selectedPage) }
    var selectModeState by remember { mutableStateOf(selectMode) }

    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            TabsTrayBanner(
                selectMode = selectModeState,
                selectedPage = selectedPageState,
                normalTabCount = normalTabCount,
                isInDebugMode = true,
                onTabPageIndicatorClicked = { page ->
                    selectedPageState = page
                },
                onExitSelectModeClick = { selectModeState = TabsTrayState.Mode.Normal },
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
