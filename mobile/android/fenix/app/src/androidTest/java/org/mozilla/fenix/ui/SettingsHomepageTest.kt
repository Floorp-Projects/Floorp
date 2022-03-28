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
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.FeatureSettingsHelper
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the Homepage settings menu
 *
 */
class SettingsHomepageTest {
    private lateinit var mockWebServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
        featureSettingsHelper.setJumpBackCFREnabled(false)
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()

        // resetting modified features enabled setting to default
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/24375")
    @SmokeTest
    @Test
    fun jumpBackInOptionTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.goToHomescreen {
            verifyJumpBackInSectionIsDisplayed()
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickJumpBackInButton()
        }.goBack {
            verifyJumpBackInSectionIsNotDisplayed()
        }
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/24384")
    @SmokeTest
    @Test
    fun recentBookmarksOptionTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu {
        }.bookmarkPage {
        }.goToHomescreen {
            verifyRecentBookmarksSectionIsDisplayed()
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickRecentBookmarksButton()
        }.goBack {
            verifyRecentBookmarksSectionIsNotDisplayed()
        }
    }

    @SmokeTest
    @Test
    fun startOnHomepageTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHomepageSubMenu {
            clickStartOnHomepageButton()
        }

        restartApp(activityIntentTestRule)

        homeScreen {
            verifyHomeScreen()
        }
    }

    @SmokeTest
    @Test
    fun startOnLastTabTest() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHomepageSubMenu {
            clickStartOnHomepageButton()
        }

        restartApp(activityIntentTestRule)

        homeScreen {
            verifyHomeScreen()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.goToHomescreen {
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickStartOnLastTabButton()
        }

        restartApp(activityIntentTestRule)

        browserScreen {
            verifyUrl(firstWebPage.url.toString())
        }
    }

    @SmokeTest
    @Test
    fun setWallpaperTest() {
        val wallpapers = listOf(
            "Wallpaper Item: amethyst",
            "Wallpaper Item: cerulean",
            "Wallpaper Item: sunrise"
        )

        for (wallpaper in wallpapers) {
            homeScreen {
            }.openThreeDotMenu {
            }.openCustomizeHome {
                openWallpapersMenu()
                selectWallpaper(wallpaper)
                verifySnackBarText("Wallpaper updated!")
            }.clickSnackBarViewButton {
                verifyWallpaperImageApplied(true)
            }
        }
    }
}
