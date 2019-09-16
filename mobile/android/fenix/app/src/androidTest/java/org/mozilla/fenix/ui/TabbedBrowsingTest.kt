/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of tabbed browsing
 *
 *  Including:
 *  - Opening a tab
 *  - Opening a private tab
 *  - Verifying tab list
 *  - Closing all tabs
 *
 *  TODO: Tab Collections
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
            verifyNoCollectionsText()
            verifyNoCollectionsHeader()
            verifyAddTabButton()
        }

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
            verifyTabCounter("1")
        }.openHomeScreen { }

        homeScreen {
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
            verifyPrivateSessionHeader()
            verifyPrivateSessionMessage(true)
            verifyAddTabButton()
        }

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
            verifyTabCounter("1")
        }.openHomeScreen {
            verifyExistingTabList()
            verifyShareTabsButton(true)
            verifyCloseTabsButton(true)
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
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
            verifyShareTabButton()
            verifySaveCollection()
        }.closeAllTabs {
            verifyNoCollectionsHeader()
            verifyNoCollectionsText()
            verifyNoTabsOpenedHeader()
            verifyNoTabsOpenedText()
        }
    }
}
