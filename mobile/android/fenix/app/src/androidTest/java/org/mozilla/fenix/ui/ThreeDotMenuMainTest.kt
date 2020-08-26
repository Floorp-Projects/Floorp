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
    fun threeDotMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
            verifySettingsButton()
            verifyBookmarksButton()
            verifyHistoryButton()
            verifyHelpButton()
            verifyWhatsNewButton()
        }.openSettings {
            verifySettingsView()
        }.goBack {
        }.openThreeDotMenu {
        }.openHelp {
            verifyHelpUrl()
        }.openTabDrawer {
        }.openNewTab {
        }.dismiss {
        }.openThreeDotMenu {
        }.openWhatsNew {
            verifyWhatsNewURL()
        }.openTabDrawer {
        }.openNewTab {
        }.dismiss { }

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
