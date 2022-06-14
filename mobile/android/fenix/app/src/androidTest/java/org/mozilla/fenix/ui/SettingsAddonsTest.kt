/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.view.View
import androidx.test.espresso.IdlingRegistry
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
import org.mozilla.fenix.helpers.ViewVisibilityIdlingResource
import org.mozilla.fenix.ui.robots.addonsMenu
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the functionality of installing or removing addons
 *
 */
class SettingsAddonsTest {
    private lateinit var mockWebServer: MockWebServer
    private var addonsListIdlingResource: RecyclerViewIdlingResource? = null
    private var addonContainerIdlingResource: ViewVisibilityIdlingResource? = null

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule()

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

        if (addonsListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(addonsListIdlingResource!!)
        }

        if (addonContainerIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(addonContainerIdlingResource!!)
        }
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
            addonsListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.add_ons_list), 1)
            IdlingRegistry.getInstance().register(addonsListIdlingResource!!)
            verifyAddonsItems()
        }
    }

    // Installs an add-on from the Add-ons menu and verifies the prompts
    @Test
    fun installAddonTest() {
        val addonName = "uBlock Origin"

        homeScreen {}
            .openThreeDotMenu {}
            .openAddonsManagerMenu {
                addonsListIdlingResource =
                    RecyclerViewIdlingResource(
                        activityTestRule.activity.findViewById(R.id.add_ons_list),
                        1
                    )
                IdlingRegistry.getInstance().register(addonsListIdlingResource!!)
                clickInstallAddon(addonName)
                IdlingRegistry.getInstance().unregister(addonsListIdlingResource!!)
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
            addonContainerIdlingResource = ViewVisibilityIdlingResource(
                activityTestRule.activity.findViewById(R.id.addon_container),
                View.VISIBLE
            )
            IdlingRegistry.getInstance().register(addonContainerIdlingResource!!)
        }.removeAddon {
            IdlingRegistry.getInstance().unregister(addonContainerIdlingResource!!)
            verifyAddonCanBeInstalled(addonName)
        }
    }

    @SmokeTest
    @Test
    // Installs uBlock add-on and checks that the app doesn't crash while loading pages with trackers
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
