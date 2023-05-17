/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.getEnhancedTrackingProtectionAsset
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.registerAndCleanupIdlingResources
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestHelper.waitUntilSnackbarGone
import org.mozilla.fenix.ui.robots.addonsMenu
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the functionality of installing or removing addons
 *
 */
class SettingsAddonsTest {
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
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
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.add_ons_list), 1),
            ) {
                verifyAddonsItems()
            }
        }
    }

    // Installs an add-on from the Add-ons menu and verifies the prompts
    @Test
    fun installAddonTest() {
        val addonName = "uBlock Origin"

        homeScreen {}
            .openThreeDotMenu {}
            .openAddonsManagerMenu {
                registerAndCleanupIdlingResources(
                    RecyclerViewIdlingResource(
                        activityTestRule.activity.findViewById(R.id.add_ons_list),
                        1,
                    ),
                ) {
                    clickInstallAddon(addonName)
                }
                verifyAddonPermissionPrompt(addonName)
                cancelInstallAddon()
                clickInstallAddon(addonName)
                acceptPermissionToInstallAddon()
                verifyAddonInstallCompleted(addonName, activityTestRule)
                verifyAddonInstallCompletedPrompt(addonName)
                closeAddonInstallCompletePrompt()
                verifyAddonIsInstalled(addonName)
                verifyEnabledTitleDisplayed()
            }
    }

    // Installs an addon, then uninstalls it
    @Test
    fun verifyAddonsCanBeUninstalled() {
        val addonName = "uBlock Origin"

        addonsMenu {
            installAddon(addonName)
            verifyAddonInstallCompleted(addonName, activityTestRule)
            closeAddonInstallCompletePrompt()
        }.openDetailedMenuForAddon(addonName) {
        }.removeAddon(activityTestRule) {
            verifySnackBarText("Successfully uninstalled $addonName")
            waitUntilSnackbarGone()
        }.goBack {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            verifyAddonCanBeInstalled(addonName)
        }
    }

    // Installs uBlock add-on and checks that the app doesn't crash while loading pages with trackers
    @SmokeTest
    @Test
    fun noCrashWithAddonInstalledTest() {
        // setting ETP to Strict mode to test it works with add-ons
        activityTestRule.activity.settings().setStrictETP()

        val addonName = "uBlock Origin"
        val trackingProtectionPage = getEnhancedTrackingProtectionAsset(mockWebServer)

        addonsMenu {
            installAddon(addonName)
            verifyAddonInstallCompleted(addonName, activityTestRule)
            closeAddonInstallCompletePrompt()
        }.goBack {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionPage.url) {
            verifyUrl(trackingProtectionPage.url.toString())
        }
    }

    @SmokeTest
    @Test
    fun useAddonsInPrivateModeTest() {
        val addonName = "uBlock Origin"
        val genericPage = getGenericAsset(mockWebServer, 1)

        addonsMenu {
            installAddon(addonName)
            verifyAddonInstallCompleted(addonName, activityTestRule)
            selectAllowInPrivateBrowsing()
            closeAddonInstallCompletePrompt()
        }.goBack {
        }.togglePrivateBrowsingMode()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyPageContent(genericPage.content)
        }.openThreeDotMenu {
            openAddonsSubList()
            verifyAddonAvailableInMainMenu(addonName)
        }
    }
}
