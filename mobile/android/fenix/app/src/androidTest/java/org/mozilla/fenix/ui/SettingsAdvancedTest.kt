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
    private val youTubeSchemaLink = itemContainingText("Youtube schema link")
    private val youTubeFullLink = itemContainingText("Youtube full link")
    private val playStoreLink = itemContainingText("Playstore link")
    private val playStoreUrl = "play.google.com"
    private val youTubePage = "vnd.youtube://".toUri()

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
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

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
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickPageObject(playStoreLink)
            waitForPageToLoad()
            verifyUrl(playStoreUrl)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2121052
    // Assumes Youtube is installed and enabled
    @Test
    fun privateBrowsingNeverOpenLinkInAppTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

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
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickPageObject(playStoreLink)
            waitForPageToLoad()
            verifyUrl(playStoreUrl)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2121045
    // Assumes Youtube is installed and enabled
    @SmokeTest
    @Test
    fun askBeforeOpeningLinkInAppCancelTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

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
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickPageObject(youTubeFullLink)
            verifyOpenLinkInAnotherAppPrompt()
            clickPageObject(itemWithResIdAndText("android:id/button2", "CANCEL"))
            waitForPageToLoad()
            verifyUrl("youtube")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2288347
    // Assumes Youtube is installed and enabled
    @SmokeTest
    @Test
    fun askBeforeOpeningLinkInAppOpenTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

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
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickPageObject(youTubeSchemaLink)
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
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

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
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickPageObject(youTubeFullLink)
            verifyPrivateBrowsingOpenLinkInAnotherAppPrompt(
                url = "youtube",
                pageObject = youTubeFullLink,
            )
            clickPageObject(itemWithResIdAndText("android:id/button2", "CANCEL"))
            waitForPageToLoad()
            verifyUrl("youtube")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2288350
    // Assumes Youtube is installed and enabled
    @Test
    fun privateBrowsingAskBeforeOpeningLinkInAppOpenTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

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
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickPageObject(youTubeSchemaLink)
            verifyPrivateBrowsingOpenLinkInAnotherAppPrompt(
                url = "youtube",
                pageObject = youTubeSchemaLink,
            )
            clickPageObject(itemWithResIdAndText("android:id/button1", "OPEN"))
            mDevice.waitForIdle()
            assertYoutubeAppOpens()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1058618
    // Assumes Youtube is installed and enabled
    @Test
    fun alwaysOpenLinkInAppTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

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
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickPageObject(youTubeSchemaLink)
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

        navigationToolbar {
        }.enterURLAndEnterToBrowser(youTubePage) {
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

        navigationToolbar {
        }.enterURLAndEnterToBrowser(youTubePage) {
            waitForPageToLoad()
            verifyOpenLinksInAppsCFRExists(true)
        }.clickOpenLinksInAppsGoToSettingsCFRButton {
            verifyOpenLinksInAppsButton()
        }
    }
}
