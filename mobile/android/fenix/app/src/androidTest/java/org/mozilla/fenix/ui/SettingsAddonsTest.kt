package org.mozilla.fenix.ui

import org.mozilla.fenix.helpers.TestAssetHelper

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import okhttp3.mockwebserver.MockWebServer
import org.junit.Rule
import org.junit.Before
import org.junit.After
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the functionality of installing or removing addons
 *
 */

class SettingsAddonsTest {
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

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // Walks through settings add-ons menu to ensure all items are present
    @Test
    fun settingsAddonsItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifyAdvancedHeading()
            verifyAddons()
        }.openAddonsManagerMenu {
            verifyAddonsItems()
        }
    }

    // Opens a webpage and installs an add-on from the three-dot menu
    @Test
    fun installAddonFromThreeDotMenu() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val addonName = "uBlock Origin"

        navigationToolbar {
        }.openNewTabAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            clickInstallAddon(addonName)
            verifyAddonPrompt(addonName)
            cancelInstallAddon()
            clickInstallAddon(addonName)
            acceptInstallAddon()
            verifyDownloadAddonPrompt(addonName, activityTestRule)
        }
    }

    // Opens the addons settings menu, installs an addon, then uninstalls
    @Test
    fun verifyAddonsCanBeUninstalled() {
        val addonName = "uBlock Origin"

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifyAdvancedHeading()
            verifyAddons()
        }.openAddonsManagerMenu {
            clickInstallAddon(addonName)
            acceptInstallAddon()
            verifyDownloadAddonPrompt(addonName, activityTestRule)
        }.openDetailedMenuForAddon(addonName) {
            verifyCurrentAddonMenu()
        }.removeAddon {
        }
    }
}
