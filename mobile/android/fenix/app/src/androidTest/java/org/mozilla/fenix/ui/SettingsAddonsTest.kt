package org.mozilla.fenix.ui

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.View
import androidx.test.espresso.IdlingRegistry
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.ViewVisibilityIdlingResource
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the functionality of installing or removing addons
 *
 */

class SettingsAddonsTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private lateinit var mockWebServer: MockWebServer
    private var addonsListIdlingResource: RecyclerViewIdlingResource? = null
    private var addonContainerIdlingResource: ViewVisibilityIdlingResource? = null

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

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

    // Opens a webpage and installs an add-on from the three-dot menu
    @Test
    fun installAddonFromThreeDotMenu() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val addonName = "uBlock Origin"

        navigationToolbar {}
            .enterURLAndEnterToBrowser(defaultWebPage.url) {}
            .openThreeDotMenu {}
            .openAddonsManagerMenu {
                addonsListIdlingResource =
                    RecyclerViewIdlingResource(
                        activityTestRule.activity.findViewById(R.id.add_ons_list),
                        1
                    )
                IdlingRegistry.getInstance().register(addonsListIdlingResource!!)
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
            addonsListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.add_ons_list), 1)
            IdlingRegistry.getInstance().register(addonsListIdlingResource!!)
            clickInstallAddon(addonName)
            acceptInstallAddon()
            verifyDownloadAddonPrompt(addonName, activityTestRule)
            IdlingRegistry.getInstance().unregister(addonsListIdlingResource!!)
        }.openDetailedMenuForAddon(addonName) {
            addonContainerIdlingResource = ViewVisibilityIdlingResource(
                activityTestRule.activity.findViewById(R.id.addon_container),
                View.VISIBLE
            )
            IdlingRegistry.getInstance().register(addonContainerIdlingResource!!)
        }.removeAddon {
        }
    }
}
