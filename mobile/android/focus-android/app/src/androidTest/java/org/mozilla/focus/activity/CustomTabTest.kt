/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.focus.activity

import androidx.lifecycle.Lifecycle
import androidx.test.core.app.launchActivity
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.rule.ActivityTestRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.browserScreen
import org.mozilla.focus.activity.robots.customTab
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MockWebServerHelper
import org.mozilla.focus.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.focus.helpers.TestAssetHelper.getGenericTabAsset
import org.mozilla.focus.helpers.TestHelper.createCustomTabIntent
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
class CustomTabTest {
    private lateinit var webServer: MockWebServer
    private val MENU_ITEM_LABEL = "TestItem4223"
    private val ACTION_BUTTON_DESCRIPTION = "TestButton"
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    val activityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java,
        true,
        false,
    )

    @Before
    fun setUp() {
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        webServer = MockWebServer().apply {
            dispatcher = MockWebServerHelper.AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        try {
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @Ignore("Crashing, see: https://github.com/mozilla-mobile/focus-android/issues/6437")
    @SmokeTest
    @Test
    fun testCustomTabUI() {
        val customTabPage = getGenericAsset(webServer)
        val customTabActivity =
            launchActivity<IntentReceiverActivity>(
                createCustomTabIntent(customTabPage.url, MENU_ITEM_LABEL, ACTION_BUTTON_DESCRIPTION),
            )

        browserScreen {
            progressBar.waitUntilGone(waitingTime)
            verifyPageContent(customTabPage.content)
            verifyPageURL(customTabPage.url)
        }

        customTab {
            verifyCustomTabActionButton(ACTION_BUTTON_DESCRIPTION)
            verifyShareButtonIsDisplayed()
            openCustomTabMenu()
            verifyTheStandardMenuItems()
            verifyCustomMenuItem(MENU_ITEM_LABEL)
            // Close the menu and close the tab
            mDevice.pressBack()
            closeCustomTab()
            assertEquals(Lifecycle.State.DESTROYED, customTabActivity.state)
        }
    }

    @SmokeTest
    @Test
    @Ignore("See https://github.com/mozilla-mobile/focus-android/issues/7580")
    fun openCustomTabInFocusTest() {
        val browserPage = getGenericAsset(webServer)
        val customTabPage = getGenericTabAsset(webServer, 1)

        launchActivity<IntentReceiverActivity>()
        homeScreen {
            // The new onboarding moved to experiments, remove comment when implemented again in Focus
            // clickStartBrowsing()
            skipFirstRun()
        }

        searchScreen {
        }.loadPage(browserPage.url) {
            verifyPageURL(browserPage.url)
        }

        launchActivity<IntentReceiverActivity>(createCustomTabIntent(customTabPage.url))
        customTab {
            progressBar.waitUntilGone(waitingTime)
            verifyPageURL(customTabPage.url)
            openCustomTabMenu()
        }.clickOpenInFocusButton {
            verifyPageURL(customTabPage.url)
            mDevice.pressBack()
            verifyPageURL(browserPage.url)
        }
    }

    @SmokeTest
    @Test
    fun customTabNavigationButtonsTest() {
        val firstPage = getGenericTabAsset(webServer, 1)
        val secondPage = getGenericTabAsset(webServer, 2)

        launchActivity<IntentReceiverActivity>(createCustomTabIntent(firstPage.url))
        customTab {
            verifyPageContent(firstPage.content)
            clickLinkMatchingText("Tab 2")
            verifyPageURL(secondPage.url)
        }.openCustomTabMenu {
        }.pressBack {
            progressBar.waitUntilGone(waitingTime)
            verifyPageURL(firstPage.url)
        }.openMainMenu {
        }.pressForward {
            verifyPageURL(secondPage.url)
        }.openMainMenu {
        }.clickReloadButton {
            verifyPageContent(secondPage.content)
        }
    }
}
