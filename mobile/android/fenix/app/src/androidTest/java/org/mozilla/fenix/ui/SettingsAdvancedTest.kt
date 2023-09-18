/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.assertYoutubeAppOpens
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the advanced section in Settings
 *
 */

class SettingsAdvancedTest {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2092699
    // Walks through settings menu and sub-menus to ensure all items are present
    @Test
    fun verifyAdvancedSettingsSectionItemsTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2121046
    // Assumes Youtube is installed and enabled
    @SmokeTest
    @Test
    fun neverOpenLinkInAppTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2121052
    // Assumes Youtube is installed and enabled
    @Test
    fun privateBrowsingNeverOpenLinkInAppTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2121045
    // Assumes Youtube is installed and enabled
    @SmokeTest
    @Test
    fun askBeforeOpeningLinkInAppCancelTest() {
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
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2288347
    // Assumes Youtube is installed and enabled
    @SmokeTest
    @Test
    fun askBeforeOpeningLinkInAppOpenTest() {
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
            clickPageObject(itemWithResIdAndText("android:id/button1", "OPEN"))
            mDevice.waitForIdle()
            assertYoutubeAppOpens()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2121051
    // Assumes Youtube is installed and enabled
    @Test
    fun privateBrowsingAskBeforeOpeningLinkInAppCancelTest() {
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
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2288350
    // Assumes Youtube is installed and enabled
    @Test
    fun privateBrowsingAskBeforeOpeningLinkInAppOpenTest() {
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
            clickPageObject(itemWithResIdAndText("android:id/button1", "OPEN"))
            mDevice.waitForIdle()
            assertYoutubeAppOpens()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1058618
    // Assumes Youtube is installed and enabled
    @Test
    fun alwaysOpenLinkInAppTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1058617
    @Test
    fun dismissOpenLinksInAppCFRTest() {
        activityIntentTestRule.applySettingsExceptions {
            it.isOpenInAppBannerEnabled = true
        }
        val defaultWebPage = "https://m.youtube.com/"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.toUri()) {
            waitForPageToLoad()
            verifyOpenLinksInAppsCFRExists(true)
            clickOpenLinksInAppsDismissCFRButton()
            verifyOpenLinksInAppsCFRExists(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2288331
    @Test
    fun goToSettingsFromOpenLinksInAppCFRTest() {
        activityIntentTestRule.applySettingsExceptions {
            it.isOpenInAppBannerEnabled = true
        }
        val defaultWebPage = "https://m.youtube.com/"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.toUri()) {
            waitForPageToLoad()
            verifyOpenLinksInAppsCFRExists(true)
        }.clickOpenLinksInAppsGoToSettingsCFRButton {
            verifyOpenLinksInAppsButton()
        }
    }
}
