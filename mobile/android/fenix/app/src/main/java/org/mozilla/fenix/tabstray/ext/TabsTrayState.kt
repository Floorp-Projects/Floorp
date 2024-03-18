/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import android.content.res.Resources
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayTestTag

/**
 * A helper to check if we're in [Mode.Select] mode.
 */
fun Mode.isSelect() = this is Mode.Select

/**
 * Returns the list of menu items corresponding to the selected mode
 *
 * @param resources The resources used to provide the strings for the menu item titles.
 * @param shouldShowInactiveButton Whether or not to show the inactive tabs menu item.
 * @param selectedPage The currently selected page.
 * @param normalTabCount The normal tabs number.
 * @param privateTabCount The private tabs number.
 * @param onBookmarkSelectedTabsClick Invoked when user interacts with the bookmark menu item.
 * @param onCloseSelectedTabsClick Invoked when user interacts with the close menu item.
 * @param onMakeSelectedTabsInactive Invoked when user interacts with the make inactive menu item.
 * @param onTabSettingsClick Invoked when user interacts with the tab settings menu.
 * @param onRecentlyClosedClick Invoked when user interacts with the recently closed menu item.
 * @param onEnterMultiselectModeClick Invoked when user enters the multiselect mode.
 * @param onShareAllTabsClick Invoked when user interacts with the share all menu item.
 * @param onDeleteAllTabsClick Invoked when user interacts with the delete all menu item.
 * @param onAccountSettingsClick Invoked when user interacts with the account settings.
 */
@Suppress("LongParameterList")
fun Mode.getMenuItems(
    resources: Resources,
    shouldShowInactiveButton: Boolean,
    selectedPage: Page,
    normalTabCount: Int,
    privateTabCount: Int,
    onBookmarkSelectedTabsClick: () -> Unit,
    onCloseSelectedTabsClick: () -> Unit,
    onMakeSelectedTabsInactive: () -> Unit,
    onTabSettingsClick: () -> Unit,
    onRecentlyClosedClick: () -> Unit,
    onEnterMultiselectModeClick: () -> Unit,
    onShareAllTabsClick: () -> Unit,
    onDeleteAllTabsClick: () -> Unit,
    onAccountSettingsClick: () -> Unit,
): List<MenuItem> {
    return if (this.isSelect()) {
        generateMultiSelectBannerMenuItems(
            resources = resources,
            shouldShowInactiveButton = shouldShowInactiveButton,
            onBookmarkSelectedTabsClick = onBookmarkSelectedTabsClick,
            onCloseSelectedTabsClick = onCloseSelectedTabsClick,
            onMakeSelectedTabsInactive = onMakeSelectedTabsInactive,
        )
    } else {
        generateSingleSelectBannerMenuItems(
            resources = resources,
            selectedPage = selectedPage,
            normalTabCount = normalTabCount,
            privateTabCount = privateTabCount,
            onTabSettingsClick = onTabSettingsClick,
            onRecentlyClosedClick = onRecentlyClosedClick,
            onEnterMultiselectModeClick = onEnterMultiselectModeClick,
            onShareAllTabsClick = onShareAllTabsClick,
            onDeleteAllTabsClick = onDeleteAllTabsClick,
            onAccountSettingsClick = onAccountSettingsClick,
        )
    }
}

/**
 *  Builds the menu items list when in multiselect mode
 */
private fun generateMultiSelectBannerMenuItems(
    resources: Resources,
    shouldShowInactiveButton: Boolean,
    onBookmarkSelectedTabsClick: () -> Unit,
    onCloseSelectedTabsClick: () -> Unit,
    onMakeSelectedTabsInactive: () -> Unit,
): List<MenuItem> {
    val menuItems = mutableListOf(
        MenuItem(
            title = resources.getString(R.string.tab_tray_multiselect_menu_item_bookmark),
            onClick = onBookmarkSelectedTabsClick,
        ),
        MenuItem(
            title = resources.getString(R.string.tab_tray_multiselect_menu_item_close),
            onClick = onCloseSelectedTabsClick,
        ),
    )
    if (shouldShowInactiveButton) {
        menuItems.add(
            MenuItem(
                title = resources.getString(R.string.inactive_tabs_menu_item),
                onClick = onMakeSelectedTabsInactive,
            ),
        )
    }

    return menuItems
}

/**
 *  Builds the menu items list when in normal mode
 */
@Suppress("LongParameterList")
private fun generateSingleSelectBannerMenuItems(
    resources: Resources,
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
        title = resources.getString(R.string.tab_tray_menu_tab_settings),
        testTag = TabsTrayTestTag.tabSettings,
        onClick = onTabSettingsClick,
    )
    val recentlyClosedTabsItem = MenuItem(
        title = resources.getString(R.string.tab_tray_menu_recently_closed),
        testTag = TabsTrayTestTag.recentlyClosedTabs,
        onClick = onRecentlyClosedClick,
    )
    val enterSelectModeItem = MenuItem(
        title = resources.getString(R.string.tabs_tray_select_tabs),
        testTag = TabsTrayTestTag.selectTabs,
        onClick = onEnterMultiselectModeClick,
    )
    val shareAllTabsItem = MenuItem(
        title = resources.getString(R.string.tab_tray_menu_item_share),
        testTag = TabsTrayTestTag.shareAllTabs,
        onClick = onShareAllTabsClick,
    )
    val deleteAllTabsItem = MenuItem(
        title = resources.getString(R.string.tab_tray_menu_item_close),
        testTag = TabsTrayTestTag.closeAllTabs,
        onClick = onDeleteAllTabsClick,
    )
    val accountSettingsItem = MenuItem(
        title = resources.getString(R.string.tab_tray_menu_account_settings),
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
