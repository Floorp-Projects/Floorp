/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.tabstray.ext.getMenuItems
import org.mozilla.fenix.tabstray.ext.isSelect

@RunWith(AndroidJUnit4::class)
class TabsTrayStateTest {

    private lateinit var store: TabsTrayStore

    @Before
    fun setup() {
        store = TabsTrayStore()
    }

    @Test
    fun `WHEN entering select mode THEN isSelected extension method returns true`() {
        store.dispatch(TabsTrayAction.EnterSelectMode)
        store.waitUntilIdle()

        Assert.assertTrue(store.state.mode.isSelect())
    }

    @Test
    fun `WHEN entering normal mode THEN isSelected extension method returns false`() {
        store.dispatch(TabsTrayAction.ExitSelectMode)
        store.waitUntilIdle()

        Assert.assertFalse(store.state.mode.isSelect())
    }

    @Test
    fun `GIVEN select mode is selected and show the inactive button is true WHEN entering any page THEN return 3 items`() {
        val menuItems = initMenuItems(
            mode = TabsTrayState.Mode.Select(emptySet()),
            shouldShowInactiveButton = true,
        )
        Assert.assertEquals(menuItems.size, 3)
        Assert.assertEquals(
            menuItems[0].title,
            testContext.getString(R.string.tab_tray_multiselect_menu_item_bookmark),
        )
        Assert.assertEquals(
            menuItems[1].title,
            testContext.getString(R.string.tab_tray_multiselect_menu_item_close),
        )
        Assert.assertEquals(
            menuItems[2].title,
            testContext.getString(R.string.inactive_tabs_menu_item),
        )
    }

    @Test
    fun `GIVEN select mode is selected and show the inactive button is false WHEN entering any page THEN return 2 menu items`() {
        val menuItems = initMenuItems(
            mode = TabsTrayState.Mode.Select(emptySet()),
        )
        Assert.assertEquals(menuItems.size, 2)
        Assert.assertEquals(
            menuItems[0].title,
            testContext.getString(R.string.tab_tray_multiselect_menu_item_bookmark),
        )
        Assert.assertEquals(
            menuItems[1].title,
            testContext.getString(R.string.tab_tray_multiselect_menu_item_close),
        )
    }

    @Test
    fun `GIVEN normal mode is selected and no normal tabs are opened WHEN entering normal page THEN return 2 menu items`() {
        val menuItems = initMenuItems(
            mode = TabsTrayState.Mode.Normal,
        )
        Assert.assertEquals(menuItems.size, 2)
        Assert.assertEquals(
            menuItems[0].title,
            testContext.getString(R.string.tab_tray_menu_tab_settings),
        )
        Assert.assertEquals(
            menuItems[1].title,
            testContext.getString(R.string.tab_tray_menu_recently_closed),
        )
    }

    @Test
    fun `GIVEN normal mode is selected and multiple normal tabs are opened WHEN entering normal page THEN return 5 menu items`() {
        val menuItems = initMenuItems(
            mode = TabsTrayState.Mode.Normal,
            normalTabCount = 3,
        )
        Assert.assertEquals(menuItems.size, 5)
        Assert.assertEquals(
            menuItems[0].title,
            testContext.getString(R.string.tabs_tray_select_tabs),
        )
        Assert.assertEquals(
            menuItems[1].title,
            testContext.getString(R.string.tab_tray_menu_item_share),
        )
        Assert.assertEquals(
            menuItems[2].title,
            testContext.getString(R.string.tab_tray_menu_tab_settings),
        )
        Assert.assertEquals(
            menuItems[3].title,
            testContext.getString(R.string.tab_tray_menu_recently_closed),
        )
        Assert.assertEquals(
            menuItems[4].title,
            testContext.getString(R.string.tab_tray_menu_item_close),
        )
    }

    @Test
    fun `GIVEN normal mode is selected and no private tabs are opened WHEN entering private page THEN return 2 menu items`() {
        val menuItems = initMenuItems(
            mode = TabsTrayState.Mode.Normal,
            selectedPage = Page.PrivateTabs,
        )
        Assert.assertEquals(menuItems.size, 2)
        Assert.assertEquals(
            menuItems[0].title,
            testContext.getString(R.string.tab_tray_menu_tab_settings),
        )
        Assert.assertEquals(
            menuItems[1].title,
            testContext.getString(R.string.tab_tray_menu_recently_closed),
        )
    }

    @Test
    fun `GIVEN normal mode is selected and multiple private tabs are opened WHEN entering private page THEN return 3 menu items`() {
        val menuItems = initMenuItems(
            mode = TabsTrayState.Mode.Normal,
            selectedPage = Page.PrivateTabs,
            privateTabCount = 6,
        )
        Assert.assertEquals(menuItems.size, 3)
        Assert.assertEquals(
            menuItems[0].title,
            testContext.getString(R.string.tab_tray_menu_tab_settings),
        )
        Assert.assertEquals(
            menuItems[1].title,
            testContext.getString(R.string.tab_tray_menu_recently_closed),
        )
        Assert.assertEquals(
            menuItems[2].title,
            testContext.getString(R.string.tab_tray_menu_item_close),
        )
    }

    @Test
    fun `GIVEN normal mode is selected WHEN entering synced page THEN return 2 menu items`() {
        val menuItems = initMenuItems(
            mode = TabsTrayState.Mode.Normal,
            selectedPage = Page.SyncedTabs,
        )
        Assert.assertEquals(menuItems.size, 2)
        Assert.assertEquals(
            menuItems[0].title,
            testContext.getString(R.string.tab_tray_menu_account_settings),
        )
        Assert.assertEquals(
            menuItems[1].title,
            testContext.getString(R.string.tab_tray_menu_recently_closed),
        )
    }

    private fun initMenuItems(
        mode: TabsTrayState.Mode,
        shouldShowInactiveButton: Boolean = false,
        selectedPage: Page = Page.NormalTabs,
        normalTabCount: Int = 0,
        privateTabCount: Int = 0,
    ): List<MenuItem> =
        mode.getMenuItems(
            resources = testContext.resources,
            shouldShowInactiveButton = shouldShowInactiveButton,
            selectedPage = selectedPage,
            normalTabCount = normalTabCount,
            privateTabCount = privateTabCount,
            onBookmarkSelectedTabsClick = {},
            onCloseSelectedTabsClick = {},
            onMakeSelectedTabsInactive = {},
            onTabSettingsClick = {},
            onRecentlyClosedClick = {},
            onEnterMultiselectModeClick = {},
            onShareAllTabsClick = {},
            onDeleteAllTabsClick = {},
            onAccountSettingsClick = {},
        )
}
