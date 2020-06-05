/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.sendSingleTapToScreen
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
 *
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
            verifyPageContent(defaultWebPage.content)
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
            verifyPageContent(defaultWebPage.content)
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
            verifyPageContent(defaultWebPage.content)
        }.openTabDrawer {
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
            verifyShareTabButton()
            verifySaveCollection()
        }.closeAllTabs {
            verifyNoTabsOpened()
        }.openHomeScreen {
        }

        // Repeat for Private Tabs
        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openTabDrawer {
            verifyPrivateModeSelected()
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
        }.closeAllTabs {
            verifyNoTabsOpened()
        }.openHomeScreen {
            verifyPrivateSessionMessage()
        }
    }

    @Test
    @Ignore("For some reason this intermittently fails with the drawer :(")
    fun closeTabTest() {
        var genericURLS = TestAssetHelper.getGenericAssets(mockWebServer)

        genericURLS.forEachIndexed { index, element ->
            navigationToolbar {
            }.openNewTabAndEnterToBrowser(element.url) {
                verifyPageContent(element.content)
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                closeTabViaXButton("Test_Page_${index + 1}")
                verifySnackBarText("Tab closed")
                snackBarButtonClick("UNDO")
//                verifyExistingOpenTabs("Test_Page_${index + 1}")
//                verifyCloseTabsButton("Test_Page_${index + 1}")
//                swipeTabRight("Test_Page_${index + 1}")
//                verifySnackBarText("Tab closed")
//                snackBarButtonClick("UNDO")
//                verifyExistingOpenTabs("Test_Page_${index + 1}")
//                verifyCloseTabsButton("Test_Page_${index + 1}")
//                swipeTabLeft("Test_Page_${index + 1}")
//                verifySnackBarText("Tab closed")
//                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
            }.openHomeScreen {
            }
        }
    }

    @Test
    @Ignore("For some reason this intermittently fails with the drawer :(")
    fun closePrivateTabTest() {
        var genericURLS = TestAssetHelper.getGenericAssets(mockWebServer)

        homeScreen { }.togglePrivateBrowsingMode()
        genericURLS.forEachIndexed { index, element ->
            navigationToolbar {
            }.openNewTabAndEnterToBrowser(element.url) {
                verifyPageContent(element.content)
            }.openTabDrawer {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                closeTabViaXButton("Test_Page_${index + 1}")
                verifySnackBarText("Private tab closed")
                snackBarButtonClick("UNDO")
//                verifyExistingOpenTabs("Test_Page_${index + 1}")
//                verifyCloseTabsButton("Test_Page_${index + 1}")
//                swipeTabRight("Test_Page_${index + 1}")
//                verifySnackBarText("Private tab closed")
//                snackBarButtonClick("UNDO")
//                verifyExistingOpenTabs("Test_Page_${index + 1}")
//                verifyCloseTabsButton("Test_Page_${index + 1}")
//                swipeTabLeft("Test_Page_${index + 1}")
//                verifySnackBarText("Private tab closed")
//                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
            }.openHomeScreen {
            }
        }
    }

    @Ignore("Temp disabled, intermittent test: https://github.com/mozilla-mobile/fenix/issues/9783")
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
            verifyPrivateSessionMessage()
        }
    }
}
