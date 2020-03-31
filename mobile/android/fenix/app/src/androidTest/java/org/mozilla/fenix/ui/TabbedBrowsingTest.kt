/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.ext.waitNotNull
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
 *  - Swipe to close tab
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

        homeScreen {
            verifyOpenTabsHeader()
            verifyNoTabsOpenedText()
            verifyNoTabsOpenedHeader()
            verifyNoCollectionsTextIsNotShown()
            verifyNoCollectionsHeaderIsNotShown()
            verifyAddTabButton()
        }

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
            verifyTabCounter("1")
        }.openHomeScreen { }

        homeScreen {
            // Timing issue on slow devices on Firebase
            mDevice.waitNotNull(
                Until.findObjects(By.res("org.mozilla.fenix.debug:id/item_tab")),
                TestAssetHelper.waitingTime
            )
            verifyExistingTabList()
            verifyNoCollectionsHeader()
            verifyNoCollectionsText()

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
            verifyPrivateSessionHeader()
            verifyPrivateSessionMessage(true)
            verifyAddTabButton()
        }

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
            verifyTabCounter("1")
        }.openHomeScreen {
            // Timing issue on slow devices on Firebase
            mDevice.waitNotNull(
                Until.findObjects(By.res("org.mozilla.fenix.debug:id/item_tab")),
                TestAssetHelper.waitingTime
            )
            verifyExistingTabList()
            verifyShareTabsButton(true)
            verifyCloseTabsButton("Test_Page_1")
        }.togglePrivateBrowsingMode()

        // Verify private tabs remain in private browsing mode

        homeScreen {
            verifyNoTabsOpenedHeader()
            verifyNoTabsOpenedText()
        }.togglePrivateBrowsingMode()

        homeScreen {
            verifyExistingTabList()
        }
    }

    @Test
    fun closeAllTabsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openHomeScreen { }

        homeScreen {
            // Timing issue on slow devices on Firebase
            mDevice.waitNotNull(
                Until.findObjects(By.res("org.mozilla.fenix.debug:id/item_tab")),
                TestAssetHelper.waitingTime
            )
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
            verifyShareTabButton()
            verifySaveCollection()
        }.closeAllTabs {
            verifyNoCollectionsHeaderIsNotShown()
            verifyNoCollectionsTextIsNotShown()
            verifyNoTabsOpenedHeader()
            verifyNoTabsOpenedText()
        }

        // Repeat for Private Tabs
        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }.openHomeScreen { }

        homeScreen {
            // Timing issue on slow devices on Firebase
            mDevice.waitNotNull(
                Until.findObjects(By.res("org.mozilla.fenix.debug:id/item_tab")),
                TestAssetHelper.waitingTime
            )
            verifyExistingTabList()
            verifyPrivateTabsCloseTabsButton()
        }.closeAllPrivateTabs {
            verifyPrivateSessionHeader()
            verifyPrivateSessionMessage(true)
        }
    }

    @Test
    fun closeTabTest() {
        var genericURLS = TestAssetHelper.getGenericAssets(mockWebServer)

        genericURLS.forEachIndexed { index, element ->
            navigationToolbar {
            }.openNewTabAndEnterToBrowser(element.url) {
                verifyPageContent(element.content)
            }.openHomeScreen { }

            homeScreen {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                closeTabViaXButton("Test_Page_${index + 1}")
                verifySnackBarText("Tab closed")
                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                swipeTabRight("Test_Page_${index + 1}")
                verifySnackBarText("Tab closed")
                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                swipeTabLeft("Test_Page_${index + 1}")
                verifySnackBarText("Tab closed")
                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
            }
        }
    }

    @Test
    fun closePrivateTabTest() {
        var genericURLS = TestAssetHelper.getGenericAssets(mockWebServer)

        homeScreen {
        }.togglePrivateBrowsingMode()
        genericURLS.forEachIndexed { index, element ->
            navigationToolbar {
            }.openNewTabAndEnterToBrowser(element.url) {
                verifyPageContent(element.content)
            }.openHomeScreen {
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                closeTabViaXButton("Test_Page_${index + 1}")
                verifySnackBarText("Private tab closed")
                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                swipeTabRight("Test_Page_${index + 1}")
                verifySnackBarText("Private tab closed")
                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
                swipeTabLeft("Test_Page_${index + 1}")
                verifySnackBarText("Private tab closed")
                snackBarButtonClick("UNDO")
                verifyExistingOpenTabs("Test_Page_${index + 1}")
                verifyCloseTabsButton("Test_Page_${index + 1}")
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
            verifyPrivateSessionMessage()
        }
    }
}
