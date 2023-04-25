/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import mozilla.components.concept.engine.utils.EngineReleaseChannel
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.assertYoutubeAppOpens
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.runWithCondition
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the advanced section in Settings
 *
 */

class SettingsAdvancedTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // Walks through settings menu and sub-menus to ensure all items are present
    @Test
    fun settingsAdvancedItemsTest() {
        // ADVANCED
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsToolbar()
            verifyAdvancedHeading()
            verifyAddons()
            verifyOpenLinksInAppsButton()
            verifySettingsOptionSummary("Open links in apps", "Never")
            verifyExternalDownloadManagerButton()
            verifyExternalDownloadManagerToggle(false)
            verifyLeakCanaryButton()
            verifyLeakCanaryToggle(true)
            verifyRemoteDebuggingButton()
            verifyRemoteDebuggingToggle(false)
        }
    }

    @SmokeTest
    @Test
    fun verifyOpenLinkInAppViewTest() {
        runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyOpenLinksInAppsButton()
                verifySettingsOptionSummary("Open links in apps", "Never")
            }.openOpenLinksInAppsMenu {
                verifyOpenLinksInAppsView("Never")
            }
        }
    }

    @SmokeTest
    @Test
    fun verifyOpenLinkInAppViewInPrivateBrowsingTest() {
        runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            homeScreen {
            }.togglePrivateBrowsingMode()

            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyOpenLinksInAppsButton()
                verifySettingsOptionSummary("Open links in apps", "Never")
            }.openOpenLinksInAppsMenu {
                verifyPrivateOpenLinksInAppsView("Never")
            }
        }
    }

    // Assumes Youtube is installed and enabled
    @Test
    fun neverOpenLinkInAppTest() {
        runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            val defaultWebPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyOpenLinksInAppsButton()
                verifySettingsOptionSummary("Open links in apps", "Never")
            }.openOpenLinksInAppsMenu {
                verifyOpenLinksInAppsView("Never")
            }

            exitMenu()

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                clickPageObject(itemContainingText("Youtube link"))
                waitForPageToLoad()
                verifyUrl("youtube.com")
            }
        }
    }

    // Assumes Youtube is installed and enabled
    @Test
    fun privateBrowsingNeverOpenLinkInAppTest() {
        runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            val defaultWebPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

            homeScreen {
            }.togglePrivateBrowsingMode()

            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyOpenLinksInAppsButton()
                verifySettingsOptionSummary("Open links in apps", "Never")
            }.openOpenLinksInAppsMenu {
                verifyPrivateOpenLinksInAppsView("Never")
            }

            exitMenu()

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                clickPageObject(itemContainingText("Youtube link"))
                waitForPageToLoad()
                verifyUrl("youtube.com")
            }
        }
    }

    // Assumes Youtube is installed and enabled
    @SmokeTest
    @Test
    fun askBeforeOpeningLinkInAppTest() {
        runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            val defaultWebPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyOpenLinksInAppsButton()
                verifySettingsOptionSummary("Open links in apps", "Never")
            }.openOpenLinksInAppsMenu {
                verifyOpenLinksInAppsView("Never")
                clickOpenLinkInAppOption("Ask before opening")
                verifySelectedOpenLinksInAppOption("Ask before opening")
            }.goBack {
                verifySettingsOptionSummary("Open links in apps", "Ask before opening")
            }

            exitMenu()

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                clickPageObject(itemContainingText("Youtube link"))
                verifyOpenLinkInAnotherAppPrompt()
                clickPageObject(itemWithResIdAndText("android:id/button2", "CANCEL"))
                waitForPageToLoad()
                verifyUrl("youtube.com")
            }

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                clickPageObject(itemContainingText("Youtube link"))
                verifyOpenLinkInAnotherAppPrompt()
                clickPageObject(itemWithResIdAndText("android:id/button1", "OPEN"))
                mDevice.waitForIdle()
                assertYoutubeAppOpens()
            }
        }
    }

    // Assumes Youtube is installed and enabled
    @SmokeTest
    @Test
    fun privateBrowsingAskBeforeOpeningLinkInAppTest() {
        runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            val defaultWebPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

            homeScreen {
            }.togglePrivateBrowsingMode()

            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyOpenLinksInAppsButton()
                verifySettingsOptionSummary("Open links in apps", "Never")
            }.openOpenLinksInAppsMenu {
                verifyPrivateOpenLinksInAppsView("Never")
                clickOpenLinkInAppOption("Ask before opening")
                verifySelectedOpenLinksInAppOption("Ask before opening")
            }.goBack {
                verifySettingsOptionSummary("Open links in apps", "Ask before opening")
            }

            exitMenu()

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                clickPageObject(itemContainingText("Youtube link"))
                verifyPrivateBrowsingOpenLinkInAnotherAppPrompt("youtube.com")
                clickPageObject(itemWithResIdAndText("android:id/button2", "CANCEL"))
                waitForPageToLoad()
                verifyUrl("youtube.com")
            }

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                clickPageObject(itemContainingText("Youtube link"))
                verifyPrivateBrowsingOpenLinkInAnotherAppPrompt("youtube.com")
                clickPageObject(itemWithResIdAndText("android:id/button1", "OPEN"))
                mDevice.waitForIdle()
                assertYoutubeAppOpens()
            }
        }
    }

    // Assumes Youtube is installed and enabled
    @Test
    fun alwaysOpenLinkInAppTest() {
        runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        ) {
            val defaultWebPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

            homeScreen {
            }.openThreeDotMenu {
            }.openSettings {
                verifyOpenLinksInAppsButton()
                verifySettingsOptionSummary("Open links in apps", "Never")
            }.openOpenLinksInAppsMenu {
                verifyOpenLinksInAppsView("Never")
                clickOpenLinkInAppOption("Always")
                verifySelectedOpenLinksInAppOption("Always")
            }.goBack {
                verifySettingsOptionSummary("Open links in apps", "Always")
            }

            exitMenu()

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                clickPageObject(itemContainingText("Youtube link"))
                mDevice.waitForIdle()
                assertYoutubeAppOpens()
            }
        }
    }
}
