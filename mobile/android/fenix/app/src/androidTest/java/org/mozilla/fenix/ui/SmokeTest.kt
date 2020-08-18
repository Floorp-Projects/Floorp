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
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.clickUrlbar
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 * Test Suite that contains tests defined as part of the Smoke and Sanity check defined in Test rail.
 * These tests will verify different functionalities of the app as a way to quickly detect regressions in main areas
 */

class SmokeTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
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

    @Test
    fun verifyBasicNavigationToolbarFunctionality() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                mDevice.waitForIdle()
                verifyNavURLBarItems()
            }.openNavigationToolbar {
            }.goBackToWebsite {
            }.openTabDrawer {
                verifyExistingTabList()
            }.openHomeScreen {
                verifyHomeScreen()
            }
        }
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/13217")
    @Test
    fun verifyPageMainMenuItemsListInPortraitNormalModeTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        // Add this to check openInApp and youtube is a default app available in every Android emulator/device
        val youtubeUrl = "www.youtube.com"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            verifyThreeDotMainMenuItems()
            verifySaveCollection()
        }.clickAddOnsReportSiteIssue {
            verifyUrl("webcompat.com/issues/new")
        }.openTabDrawer {
        }.openTab(defaultWebPage.title) {
        }.openThreeDotMenu {
        }.openHistory {
            verifyTestPageUrl(defaultWebPage.url)
        }.goBackToBrowser {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarksMenuView()
            verifyEmptyBookmarksList()
        }.goBackToBrowser {
        }.openThreeDotMenu {
        }.openSyncedTabs {
            verifyNavigationToolBarHeader()
            verifySyncedTabsStatus()
        }.goBack {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }.goBackToBrowser {
        }.openThreeDotMenu {
        }.openFindInPage {
            verifyFindInPageSearchBarItems()
        }.closeFindInPage {
        }.openThreeDotMenu {
        }.addToFirefoxHome {
            verifySnackBarText("Added to top sites!")
        }.openTabDrawer {
        }.openHomeScreen {
            verifyExistingTopSitesTabs(defaultWebPage.title)
        }.openTabDrawer {
        }.openTab(defaultWebPage.title) {
        }.openThreeDotMenu {
        }.openAddToHomeScreen {
            verifyShortcutNameField(defaultWebPage.title)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(defaultWebPage.title) {
        }.openThreeDotMenu {
        }.openSaveToCollection {
            verifyCollectionNameTextField()
        }.goBackToBrowser {
        }.openThreeDotMenu {
        }.bookmarkPage {
            verifySnackBarText("Bookmark saved!")
        }.openThreeDotMenu {
        }.sharePage {
            verifyShareAppsLayout()
        }.closeShareDialogReturnToPage {
        }.openThreeDotMenu {
        }.refreshPage {
            verifyUrl(defaultWebPage.url.toString())
        }.openTabDrawer {
            closeTabViaXButton(defaultWebPage.title)
        }.openHomeScreen {
            navigationToolbar {
            }.enterURLAndEnterToBrowser(youtubeUrl.toUri()) {
                verifyBlueDot()
            }.openThreeDotMenu {
                verifyOpenInAppButton()
            }
        }
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/13217")
    @Test
    fun verifyPageMainMenuItemsListInPortraitPrivateModeTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        // Add this to check openInApp and also youtube is a default app available in every Android emulator/device
        val youtubeUrl = "www.youtube.com"

        homeScreen {
            togglePrivateBrowsingModeOnOff()
            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            }.openThreeDotMenu {
                verifyThreeDotMainMenuItems()
            }.clickAddOnsReportSiteIssue {
                verifyUrl("webcompat.com/issues/new")
            }.openTabDrawer {
            }.openTab(defaultWebPage.title) {
            }.openThreeDotMenu {
            }.openHistory {
                verifyEmptyHistoryView()
            }.goBackToBrowser {
            }.openThreeDotMenu {
            }.openBookmarks {
                verifyBookmarksMenuView()
                verifyEmptyBookmarksList()
            }.goBackToBrowser {
            }.openThreeDotMenu {
            }.openSyncedTabs {
                verifyNavigationToolBarHeader()
                verifySyncedTabsStatus()
            }.goBack {
            }.openThreeDotMenu {
            }.openSettings {
                verifySettingsView()
            }.goBackToBrowser {
            }.openThreeDotMenu {
            }.openFindInPage {
                verifyFindInPageSearchBarItems()
            }.closeFindInPage {
            }.openThreeDotMenu {
            }.addToFirefoxHome {
                verifySnackBarText("Added to top sites!")
            }.openTabDrawer {
            }.openHomeScreen {
                togglePrivateBrowsingModeOnOff()
                verifyExistingTopSitesTabs(defaultWebPage.title)
                togglePrivateBrowsingModeOnOff()
            }.openTabDrawer {
            }.openTab(defaultWebPage.title) {
            }.openThreeDotMenu {
            }.openAddToHomeScreen {
                verifyShortcutNameField(defaultWebPage.title)
                clickAddShortcutButton()
                clickAddAutomaticallyButton()
            }.openHomeScreenShortcut(defaultWebPage.title) {
            }.openThreeDotMenu {
            }.bookmarkPage {
                verifySnackBarText("Bookmark saved!")
            }.openThreeDotMenu {
            }.sharePage {
                verifyShareAppsLayout()
            }.closeShareDialogReturnToPage {
            }.openThreeDotMenu {
            }.refreshPage {
                verifyUrl(defaultWebPage.url.toString())
            }.openTabDrawer {
                closeTabViaXButton(defaultWebPage.title)
            }.openHomeScreen {
                navigationToolbar {
                }.enterURLAndEnterToBrowser(youtubeUrl.toUri()) {
                    verifyBlueDot()
                }.openThreeDotMenu {
                    verifyOpenInAppButton()
                }
            }
        }
    }

    @Ignore("Flaky test: https://github.com/mozilla-mobile/fenix/issues/12899")
    @Test
    fun verifyETPToolbarShieldIconIsNotDisplayedIfETPIsOFFGloballyTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            clickEnhancedTrackingProtectionDefaults()
            verifyEnhancedTrackingProtectionOptionsGrayedOut()
        }.goBackToHomeScreen {
            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                verifyEnhancedTrackingProtectionPanelNotVisible()
            }.openThreeDotMenu {
            }.clickAddOnsReportSiteIssue {
                verifyUrl("webcompat.com/issues/new")
                verifyTabCounter("2")
            }.openTabDrawer {
            }.openHomeScreen {
            }.openThreeDotMenu {
            }.openSettings {
            }.openEnhancedTrackingProtectionSubMenu {
                clickEnhancedTrackingProtectionDefaults()
            }.goBackToHomeScreen {
            }.openTabDrawer {
            }.openTab(defaultWebPage.title) {
                clickEnhancedTrackingProtectionPanel()
                verifyEnhancedTrackingProtectionSwitch()
                // Turning off TP Switch results in adding the WebPage to exception list
                clickEnhancedTrackingProtectionSwitchOffOn()
            }
        }
    }

    @Test
    fun verifySearchEngineCanBeChangedTemporarilyUsingShortcuts() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openSearch {
            verifyKeyboardVisibility()
            clickSearchEngineButton()
            verifySearchEngineList()
            changeDefaultSearchEngine("Amazon.com")
            verifySearchEngineIcon("Amazon.com")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            // Changing search engine to Bing
        }.openHomeScreen {
        }.openSearch {
            clickSearchEngineButton()
            mDevice.waitForIdle()
            changeDefaultSearchEngine("Bing")
            verifySearchEngineIcon("Bing")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            // Changing search engine to DuckDuckGo
        }.openHomeScreen {
        }.openSearch {
            clickSearchEngineButton()
            mDevice.waitForIdle()
            changeDefaultSearchEngine("DuckDuckGo")
            verifySearchEngineIcon("DuckDuckGo")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            // Changing search engine to Twitter
        }.openHomeScreen {
        }.openSearch {
            clickSearchEngineButton()
            mDevice.waitForIdle()
            changeDefaultSearchEngine("Twitter")
            verifySearchEngineIcon("Twitter")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            // Changing search engine to Wikipedia
        }.openHomeScreen {
        }.openSearch {
            clickSearchEngineButton()
            changeDefaultSearchEngine("Wikipedia")
            verifySearchEngineIcon("Wikipedia")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            // Checking whether the next search will be with default or not
        }.openHomeScreen {
        }.openSearch {
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openNavigationToolbar {
            clickUrlbar {
                verifyDefaultSearchEngine("Google")
            }
        }
    }
}
