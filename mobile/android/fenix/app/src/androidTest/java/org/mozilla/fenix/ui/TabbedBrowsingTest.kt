/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.sendSingleTapToScreen
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
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }
    }

    // changing the device preference for Touch and Hold delay, to avoid long-clicks instead of a single-click
    companion object {
        @BeforeClass
        @JvmStatic
        fun setDevicePreference() {
            val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
            mDevice.executeShellCommand("settings put secure long_press_timeout 3000")
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun openNewTabTest() {
        homeScreen { }.dismissOnboarding()

        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
            verifyTabCounter("1")
        }.openTabDrawer {
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
            verifyShareTabButton()
            verifySaveCollection()
        }
    }

    @Test
    fun openNewPrivateTabTest() {
        homeScreen { }.dismissOnboarding()

        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen { }.togglePrivateBrowsingMode()

        homeScreen {
            verifyPrivateSessionMessage()
            verifyTabButton()
        }

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
            verifyTabCounter("1")
        }.openTabDrawer {
            verifyExistingTabList()
            verifyCloseTabsButton("Test_Page_1")
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
            verifySaveCollection()
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
        var genericURLS = TestAssetHelper.getGenericAssets(mockWebServer)

        genericURLS.forEachIndexed { index, element ->
            navigationToolbar {
            }.openNewTabAndEnterToBrowser(element.url) {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                closeTabViaXButton("Test_Page_${index + 1}")
                verifySnackBarText("Tab closed")
                snackBarButtonClick("UNDO")
            }

            mDevice.waitForIdle()

            browserScreen {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                swipeTabRight("Test_Page_${index + 1}")
                verifySnackBarText("Tab closed")
                snackBarButtonClick("UNDO")
            }

            mDevice.waitForIdle()

            browserScreen {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                swipeTabLeft("Test_Page_${index + 1}")
                verifySnackBarText("Tab closed")
                snackBarButtonClick("UNDO")
            }

            mDevice.waitForIdle()

            browserScreen {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
            }.openHomeScreen {
            }
        }
    }

    @Test
    fun closePrivateTabTest() {
        var genericURLS = TestAssetHelper.getGenericAssets(mockWebServer)

        homeScreen { }.togglePrivateBrowsingMode()
        genericURLS.forEachIndexed { index, element ->
            navigationToolbar {
            }.openNewTabAndEnterToBrowser(element.url) {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                closeTabViaXButton("Test_Page_${index + 1}")
                verifySnackBarText("Private tab closed")
                snackBarButtonClick("UNDO")
            }

            mDevice.waitForIdle()

            browserScreen {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                swipeTabRight("Test_Page_${index + 1}")
                verifySnackBarText("Private tab closed")
                snackBarButtonClick("UNDO")
            }

            mDevice.waitForIdle()

            browserScreen {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                swipeTabLeft("Test_Page_${index + 1}")
                verifySnackBarText("Private tab closed")
                snackBarButtonClick("UNDO")
            }

            mDevice.waitForIdle()

            browserScreen {
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                closeTabViaXButton("Test_Page_${index + 1}")
            }
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
            // Tap an empty spot on the app homescreen to make sure it's into focus
            sendSingleTapToScreen(20, 20)
            verifyHomeScreen()
        }
    }

    @Test
    fun verifyEmptyTabTray() {
        homeScreen { }.dismissOnboarding()

        navigationToolbar {
        }.openTabTray {
            verifyNoTabsOpened()
            verifyNewTabButton()
            verifyTabTrayOverflowMenu(false)
        }.toggleToPrivateTabs {
            verifyNoTabsOpened()
            verifyNewTabButton()
            verifyTabTrayOverflowMenu(false)
        }
    }

    @Test
    fun verifyOpenTabDetails() {
        homeScreen { }.dismissOnboarding()

        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            // verifyPageContent(defaultWebPage.content)
        }.openTabDrawer {
            verifyExistingTabList()
            verifyNewTabButton()
            verifyTabTrayOverflowMenu(true)
            verifyExistingOpenTabs(defaultWebPage.title)
            verifyCloseTabsButton(defaultWebPage.title)
        }.openHomeScreen {
        }
    }

    @Test
    fun verifyContextMenuShortcuts() {
        homeScreen { }.dismissOnboarding()

        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            // verifyPageContent(defaultWebPage.content)
        }.openTabDrawer {
            verifyExistingTabList()
            verifyNewTabButton()
            verifyTabTrayOverflowMenu(true)
            verifyExistingOpenTabs(defaultWebPage.title)
            verifyCloseTabsButton(defaultWebPage.title)
        }.closeTabDrawer {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }.closeTabFromShortcutsMenu {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()
            verifyTabButton()
            verifyPrivateSessionMessage()
            verifyHomeToolbar()
            verifyHomeComponent()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {

        }.openTabButtonShortcutsMenu {
        }.openTabFromShortcutsMenu {
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomeMenu()
            verifyHomeWordmark()
            verifyTabButton()
            verifyHomeToolbar()
            verifyHomeComponent()
        }
    }
}
