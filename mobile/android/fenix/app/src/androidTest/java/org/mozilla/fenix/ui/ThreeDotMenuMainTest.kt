/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class ThreeDotMenuMainTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private lateinit var mockWebServer: MockWebServer

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

    // Verifies the list of items in the homescreen's 3 dot main menu
    @Ignore("Failing with frequent ANR: https://bugzilla.mozilla.org/show_bug.cgi?id=1764605")
    @Test
    fun homeThreeDotMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
            verifyBookmarksButton()
            verifyHistoryButton()
            verifyDownloadsButton()
            verifyAddOnsButton()
            verifySyncSignInButton()
            verifyDesktopSite()
            verifyWhatsNewButton()
            verifyHelpButton()
            verifyCustomizeHomeButton()
            verifySettingsButton()
        }.openSettings {
            verifySettingsView()
        }.goBack {
        }.openThreeDotMenu {
        }.openCustomizeHome {
            verifyHomePageView()
        }.goBack {
        }.openThreeDotMenu {
        }.openHelp {
            verifyHelpUrl()
        }.openTabDrawer {
            closeTab()
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openWhatsNew {
            verifyWhatsNewURL()
        }.openTabDrawer {
            closeTab()
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarksMenuView()
        }.closeMenu {
        }

        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryMenuView()
        }
    }
}
