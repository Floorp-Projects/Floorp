/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import com.google.android.material.bottomsheet.BottomSheetBehavior
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade

/**
 *  Tests for verifying basic functionality of tabbed browsing
 *
 *  Including:
 *  - Opening a tab
 *  - Opening a private tab
 *  - Verifying tab list
 *  - Closing all tabs
 *  - Close tab
 *  - Swipe to close tab (temporarily disabled)
 *  - Undo close tab
 *  - Close private tabs persistent notification
 *  - Empty tab tray state
 *  - Tab tray details
 *  - Shortcut context menu navigation
 */

class TabbedBrowsingTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer

    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        activityTestRule.activity.applicationContext.settings().shouldShowJumpBackInCFR = false
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun openNewTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
            verifyTabCounter("1")
        }.openTabDrawer {
            verifyNormalModeSelected()
            verifyExistingOpenTabs("Test_Page_1")
            closeTab()
        }.openTabDrawer {
            verifyNoTabsOpened()
        }.openNewTab {
        }.submitQuery(defaultWebPage.url.toString()) {
            mDevice.waitForIdle()
            verifyTabCounter("1")
        }.openTabDrawer {
            verifyNormalModeSelected()
            verifyExistingOpenTabs("Test_Page_1")
        }
    }

    @Test
    fun openNewPrivateTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {}.togglePrivateBrowsingMode()

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
            verifyTabCounter("1")
        }.openTabDrawer {
            verifyExistingTabList()
            verifyPrivateModeSelected()
        }.toggleToNormalTabs {
            verifyNoTabsOpened()
        }.toggleToPrivateTabs {
            verifyExistingTabList()
        }
    }

    @Test
    fun closeAllTabsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
            verifyShareTabButton()
            verifySelectTabs()
        }.closeAllTabs {
            verifyNoTabsOpened()
        }

        // Repeat for Private Tabs
        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            verifyPrivateModeSelected()
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
        }.closeAllTabs {
            verifyNoTabsOpened()
        }
    }

    @Test
    fun closeTabTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(genericURL.url) {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            closeTabViaXButton("Test_Page_1")
            verifySnackBarText("Tab closed")
            snackBarButtonClick("UNDO")
        }

        mDevice.waitForIdle()

        browserScreen {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabRight("Test_Page_1")
            verifySnackBarText("Tab closed")
            snackBarButtonClick("UNDO")
        }

        mDevice.waitForIdle()

        browserScreen {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabLeft("Test_Page_1")
            verifySnackBarText("Tab closed")
            snackBarButtonClick("UNDO")
        }

        mDevice.waitForIdle()

        browserScreen {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
        }.closeTabDrawer { }
    }

    @Test
    fun closePrivateTabTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen { }.togglePrivateBrowsingMode()
        navigationToolbar {
        }.openNewTabAndEnterToBrowser(genericURL.url) {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            verifyCloseTabsButton("Test_Page_1")
            closeTabViaXButton("Test_Page_1")
            verifySnackBarText("Private tab closed")
            snackBarButtonClick("UNDO")
        }

        mDevice.waitForIdle()

        browserScreen {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabRight("Test_Page_1")
            verifySnackBarText("Private tab closed")
            snackBarButtonClick("UNDO")
        }

        mDevice.waitForIdle()

        browserScreen {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabLeft("Test_Page_1")
            verifySnackBarText("Private tab closed")
            snackBarButtonClick("UNDO")
        }

        mDevice.waitForIdle()

        browserScreen {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
        }
    }

    @Test
    fun closePrivateTabsNotificationTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.openNotification()
        }

        notificationShade {
            verifyPrivateTabsNotification()
        }.clickClosePrivateTabsNotification {
            verifyHomeScreen()
        }
    }

    @Test
    fun verifyTabTrayNotShowingStateHalfExpanded() {

        navigationToolbar {
        }.openTabTray {
            verifyNoTabsOpened()
            // With no tabs opened the state should be STATE_COLLAPSED.
            verifyBehaviorState(BottomSheetBehavior.STATE_COLLAPSED)
            // Need to ensure the halfExpandedRatio is very small so that when in STATE_HALF_EXPANDED
            // the tabTray will actually have a very small height (for a very short time) akin to being hidden.
            verifyHalfExpandedRatio()
        }.clickTopBar {
        }.waitForTabTrayBehaviorToIdle {
            // Touching the topBar would normally advance the tabTray to the next state.
            // We don't want that.
            verifyBehaviorState(BottomSheetBehavior.STATE_COLLAPSED)
        }.advanceToHalfExpandedState {
        }.waitForTabTrayBehaviorToIdle {
            // TabTray should not be displayed in STATE_HALF_EXPANDED.
            // When advancing to this state it should immediately be hidden.
            verifyTabTrayIsClosed()
        }
    }

    @Test
    fun verifyEmptyTabTray() {
        navigationToolbar {
        }.openTabTray {
            verifyNoTabsOpened()
            verifyNewTabButton()
            verifyTabTrayOverflowMenu(true)
        }.toggleToPrivateTabs {
            verifyNoTabsOpened()
            verifyNewTabButton()
            verifyTabTrayOverflowMenu(true)
        }
    }

    @Test
    fun verifyOpenTabDetails() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            verifyExistingTabList()
            verifyNewTabButton()
            verifyTabTrayOverflowMenu(true)
            verifyExistingOpenTabs(defaultWebPage.title)
            verifyCloseTabsButton(defaultWebPage.title)
        }.openNewTab {
            verifySearchBarEmpty()
            verifyKeyboardVisibility()
        }
    }

    @Test
    fun verifyContextMenuShortcuts() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }.closeTabFromShortcutsMenu {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifyKeyboardVisible()
            verifyFocusedNavigationToolbar()
            // dismiss search dialog
            homeScreen { }.pressBack()
            verifyPrivateSessionMessage()
            verifyHomeToolbar()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabButtonShortcutsMenu {
        }.openTabFromShortcutsMenu {
            verifyKeyboardVisible()
            verifyFocusedNavigationToolbar()
            // dismiss search dialog
            homeScreen { }.pressBack()
            verifyHomeWordmark()
            verifyHomeToolbar()
        }
    }
}
