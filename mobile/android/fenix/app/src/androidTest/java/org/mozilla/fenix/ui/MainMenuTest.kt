/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
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
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertYoutubeAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.runWithCondition
import org.mozilla.fenix.helpers.Constants.PackageName.PRINT_SPOOLER
import org.mozilla.fenix.helpers.DataGenerationHelper.generateRandomString
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickContextMenuItem
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.longClickPageObject
import org.mozilla.fenix.ui.robots.navigationToolbar

class MainMenuTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule =
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(translationsEnabled = true)

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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/233849
    @Test
    fun verifyTabMainMenuItemsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }.openThreeDotMenu {
            verifyPageThreeDotMainMenuItems(isRequestDesktopSiteEnabled = false)
        }
    }

    // Verifies the list of items in the homescreen's 3 dot main menu
    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/233848
    @SmokeTest
    @Test
    fun homeMainMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
            verifyHomeThreeDotMainMenuItems(isRequestDesktopSiteEnabled = false)
        }.openBookmarks {
            verifyBookmarksMenuView()
        }.goBack {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryMenuView()
        }.goBack {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }.goBack {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            verifyAddonsListIsDisplayed(true)
        }.goBack {
        }.openThreeDotMenu {
        }.openSyncSignIn {
            verifyTurnOnSyncMenu()
        }.goBack {}
        homeScreen {
        }.openThreeDotMenu {
        }.openWhatsNew {
            verifyWhatsNewURL()
        }.goToHomescreen {
        }.openThreeDotMenu {
        }.openHelp {
            verifyHelpUrl()
        }.goToHomescreen {
        }.openThreeDotMenu {
        }.openCustomizeHome {
            verifyHomePageView()
        }.goBackToHomeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2284134
    @Test
    fun openNewTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.clickNewTabButton {
            verifySearchView()
        }.submitQuery("test") {
            verifyTabCounter("2")
        }
    }

    // Device or AVD requires a Google Services Android OS installation with Play Store installed
    // Verifies the Open in app button when an app is installed
    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/387756
    @SmokeTest
    @Test
    fun openInAppFunctionalityTest() {
        val youtubeURL = "vnd.youtube://".toUri()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(youtubeURL) {
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickOpenInApp {
            assertYoutubeAppOpens()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2284323
    @Test
    fun openSyncAndSaveDataTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSyncSignIn {
            verifyTurnOnSyncMenu()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/243840
    @Test
    fun findInPageTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
            verifyFindInPageButton()
        }.openFindInPage {
            verifyFindInPageNextButton()
            verifyFindInPagePrevButton()
            verifyFindInPageCloseButton()
            enterFindInPageQuery("a")
            verifyFindNextInPageResult("1/3")
            clickFindInPageNextButton()
            verifyFindNextInPageResult("2/3")
            clickFindInPageNextButton()
            verifyFindNextInPageResult("3/3")
            clickFindInPagePrevButton()
            verifyFindPrevInPageResult("2/3")
            clickFindInPagePrevButton()
            verifyFindPrevInPageResult("1/3")
        }.closeFindInPageWithCloseButton {
            verifyFindInPageBar(false)
        }.openThreeDotMenu {
        }.openFindInPage {
            enterFindInPageQuery("3")
            verifyFindNextInPageResult("1/1")
        }.closeFindInPageWithBackButton {
            verifyFindInPageBar(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2283303
    @Test
    fun switchDesktopSiteModeOnOffTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.switchDesktopSiteMode {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }.switchDesktopSiteMode {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1314137
    @Test
    fun setDesktopSiteBeforePageLoadTest() {
        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 4)

        homeScreen {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(false)
        }.switchDesktopSiteMode {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }.closeBrowserMenuToBrowser {
            clickPageObject(MatcherHelper.itemContainingText("Link 1"))
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }.closeBrowserMenuToBrowser {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
            longClickPageObject(MatcherHelper.itemWithText("Link 2"))
            clickContextMenuItem("Open link in new tab")
            TestHelper.clickSnackbarButton("SWITCH")
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2283302
    @Test
    fun reportSiteIssueTest() {
        runWithCondition(
            // This test will not run on RC builds because the "Report site issue button" is not available.
            activityTestRule.activity.components.core.engine.version.releaseChannel !== EngineReleaseChannel.RELEASE,
        ) {
            val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            }.openThreeDotMenu {
            }.openReportSiteIssue {
                verifyUrl("webcompat.com/issues/new")
            }
        }
    }

    // Verifies the Add to home screen option in a tab's 3 dot menu
    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/410724
    @SmokeTest
    @Test
    fun addPageShortcutToHomeScreenTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val shortcutTitle = generateRandomString(5)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
        }.openThreeDotMenu {
            expandMenu()
        }.openAddToHomeScreen {
            clickCancelShortcutButton()
        }

        browserScreen {
        }.openThreeDotMenu {
            expandMenu()
        }.openAddToHomeScreen {
            verifyShortcutTextFieldTitle("Test_Page_1")
            addShortcutName(shortcutTitle)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            verifyUrl(website.url.toString())
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/329893
    @SmokeTest
    @Test
    fun mainMenuShareButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.clickShareButton {
            verifyShareTabLayout()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/233604
    @Test
    fun navigateBackAndForwardTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val nextWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(nextWebPage.url) {
            verifyUrl(nextWebPage.url.toString())
        }.openThreeDotMenu {
        }.goToPreviousPage {
            mDevice.waitForIdle()
            verifyUrl(defaultWebPage.url.toString())
        }.openThreeDotMenu {
        }.goForward {
            verifyUrl(nextWebPage.url.toString())
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2195819
    @SmokeTest
    @Test
    fun refreshPageButtonTest() {
        val refreshWebPage = TestAssetHelper.getRefreshAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(refreshWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
        }.refreshPage {
            verifyPageContent("REFRESHED")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2265657
    @Test
    fun forceRefreshPageTest() {
        val refreshWebPage = TestAssetHelper.getRefreshAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(refreshWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
        }.forceRefreshPage {
            verifyPageContent("REFRESHED")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2282411
    @Test
    fun printWebPageFromMainMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.clickPrintButton {
            assertNativeAppOpens(PRINT_SPOOLER)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2282408
    @Test
    fun printWebPageFromShareMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.clickShareButton {
        }.clickPrintButton {
            assertNativeAppOpens(PRINT_SPOOLER)
        }
    }
}
