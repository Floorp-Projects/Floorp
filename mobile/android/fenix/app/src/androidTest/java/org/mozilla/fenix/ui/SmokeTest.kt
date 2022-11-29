/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui

import android.view.View
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiDevice
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.Constants.PackageName.YOUTUBE_APP
import org.mozilla.fenix.helpers.FeatureSettingsHelperDelegate
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.TestHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.TestHelper.generateRandomString
import org.mozilla.fenix.helpers.TestHelper.registerAndCleanupIdlingResources
import org.mozilla.fenix.helpers.ViewVisibilityIdlingResource
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade
import org.mozilla.fenix.ui.robots.openEditURLView
import org.mozilla.fenix.ui.robots.searchScreen

/**
 * Test Suite that contains a part of the Smoke and Sanity tests defined in TestRail:
 * https://testrail.stage.mozaws.net/index.php?/suites/view/3192
 * Other smoke tests have been marked with the @SmokeTest annotation throughout the ui package in order to limit this class expansion.
 * These tests will verify different functionalities of the app as a way to quickly detect regressions in main areas
 */
@Suppress("ForbiddenComment")
@SmokeTest
class SmokeTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer
    private val customMenuItem = "TestMenuItem"
    private lateinit var browserStore: BrowserStore
    private val featureSettingsHelper = FeatureSettingsHelperDelegate()

    @get:Rule(order = 0)
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityIntentTestRule(),
        { it.activity },
    )

    @get: Rule(order = 1)
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java,
        true,
        false,
    )

    @Rule(order = 2)
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        // Initializing this as part of class construction, below the rule would throw a NPE
        // So we are initializing this here instead of in all related tests.
        browserStore = activityTestRule.activity.components.core.store

        // disabling the new homepage pop-up that interferes with the tests.
        featureSettingsHelper.apply {
            isJumpBackInCFREnabled = false
            isTCPCFREnabled = false
            isWallpaperOnboardingEnabled = false
        }.applyFlagUpdates()

        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()

        // resetting modified features enabled setting to default
        featureSettingsHelper.resetAllFeatureFlags()
    }

    // Verifies the first run onboarding screen
    @Test
    fun firstRunScreenTest() {
        homeScreen {
            verifyHomeScreenAppBarItems()
            verifyHomeScreenWelcomeItems()
            verifyChooseYourThemeCard(
                isDarkThemeChecked = false,
                isLightThemeChecked = false,
                isAutomaticThemeChecked = true,
            )
            verifyToolbarPlacementCard(isBottomChecked = true, isTopChecked = false)
            verifySignInToSyncCard()
            verifyPrivacyProtectionCard(isStandardChecked = true, isStrictChecked = false)
            verifyPrivacyNoticeCard()
            verifyStartBrowsingSection()
            verifyNavigationToolbarItems("0")
        }
    }

    // Verifies the functionality of the onboarding Start Browsing button
    @Test
    fun startBrowsingButtonTest() {
        homeScreen {
            verifyStartBrowsingButton()
        }.clickStartBrowsingButton {
            verifySearchView()
        }
    }

    /* Verifies the nav bar:
     - opening a web page
     - the existence of nav bar items
     - editing the url bar
     - the tab drawer button
     - opening a new search and dismissing the nav bar
    */
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
            }.openNewTab {
            }.dismissSearchBar {
                verifyHomeScreen()
            }
        }
    }

    // Verifies the list of items in a tab's 3 dot menu
    @Test
    fun verifyPageMainMenuItemsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }.openThreeDotMenu {
            verifyPageThreeDotMainMenuItems()
        }
    }

    // Could be removed when more smoke tests from the History category are added
    // Verifies the History menu opens from a tab's 3 dot menu
    @Test
    fun openMainMenuHistoryItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
        }
    }

    // Could be removed when more smoke tests from the Bookmarks category are added
    // Verifies the Bookmarks menu opens from a tab's 3 dot menu
    @Test
    fun openMainMenuBookmarksItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarksMenuView()
        }
    }

    // Verifies the Add-ons menu opens from a tab's 3 dot menu
    @Test
    fun openMainMenuAddonsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.add_ons_list), 1),
            ) {
                verifyAddonsItems()
            }
        }
    }

    // Verifies the Synced tabs menu or Sync Sign In menu opens from a tab's 3 dot menu.
    // The test is assuming we are NOT signed in.
    @Test
    fun openMainMenuSyncItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSyncSignIn {
            verifyTurnOnSyncMenu()
        }
    }

    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Verifies the Settings menu opens from a tab's 3 dot menu
    @Test
    fun openMainMenuSettingsItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }
    }

    // Verifies the Find in page option in a tab's 3 dot menu
    @Test
    fun openMainMenuFindInPageTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openFindInPage {
            verifyFindInPageSearchBarItems()
        }
    }

    // Verifies the Add to home screen option in a tab's 3 dot menu
    @Test
    fun mainMenuAddToHomeScreenTest() {
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
            verifyShortcutNameField("Test_Page_1")
            addShortcutName(shortcutTitle)
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut(shortcutTitle) {
            verifyUrl(website.url.toString())
            verifyTabCounter("1")
        }
    }

    // Verifies the Add to collection option in a tab's 3 dot menu
    @Test
    fun openMainMenuAddToCollectionTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openSaveToCollection {
            verifyCollectionNameTextField()
        }
    }

    // Verifies the Bookmark button in a tab's 3 dot menu
    @Test
    fun mainMenuBookmarkButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.bookmarkPage {
            verifySnackBarText("Bookmark saved!")
        }
    }

    // Device or AVD requires a Google Services Android OS installation with Play Store installed
    // Verifies the Open in app button when an app is installed
    @Test
    fun mainMenuOpenInAppTest() {
        val youtubeURL = "https://m.youtube.com/user/mozilla?cbrd=1"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(youtubeURL.toUri()) {
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickOpenInApp {
            assertNativeAppOpens(YOUTUBE_APP, youtubeURL)
        }
    }

    // Verifies the Desktop site toggle in a tab's 3 dot menu
    @Test
    fun mainMenuDesktopSiteTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.switchDesktopSiteMode {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }
    }

    // Verifies the Share button in a tab's 3 dot menu
    @Test
    fun mainMenuShareButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.clickShareButton {
            verifyShareTabLayout()
            verifySendToDeviceTitle()
            verifyShareALinkTitle()
        }
    }

    // Verifies the refresh button in a tab's 3 dot menu
    @Test
    fun mainMenuRefreshButtonTest() {
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

    // Verifies changing the default engine from the Search Shortcut menu
    @Test
    fun selectSearchEnginesShortcutTest() {
        val enginesList = listOf("DuckDuckGo", "Google", "Amazon.com", "Wikipedia", "Bing", "eBay")

        for (searchEngine in enginesList) {
            homeScreen {
            }.openSearch {
                verifyKeyboardVisibility()
                clickSearchEngineShortcutButton()
                verifySearchEngineList(activityTestRule)
                changeDefaultSearchEngine(activityTestRule, searchEngine)
                verifySearchEngineIcon(searchEngine)
            }.submitQuery("mozilla ") {
                verifyUrl(searchEngine)
            }.goToHomescreen { }
        }
    }

    // Saves a login, then changes it and verifies the update
    @Test
    fun updateSavedLoginTest() {
        val saveLoginTest =
            TestAssetHelper.getSaveLoginAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            verifySaveLoginPromptIsShown()
            // Click Save to save the login
            saveLoginFromPrompt("Save")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(saveLoginTest.url) {
            enterPassword("test")
            verifyUpdateLoginPromptIsShown()
            // Click Update to change the saved password
            saveLoginFromPrompt("Update")
        }.openThreeDotMenu {
        }.openSettings {
            TestHelper.scrollToElementByText("Logins and passwords")
        }.openLoginsAndPasswordSubMenu {
        }.openSavedLogins {
            verifySecurityPromptForLogins()
            tapSetupLater()
            // Verify that the login appears correctly
            verifySavedLoginFromPrompt("test@example.com")
            viewSavedLoginDetails("test@example.com")
            revealPassword()
            verifyPasswordSaved("test") // failing here locally
        }
    }

    // Verifies that a recently closed item is properly opened
    @Test
    fun openRecentlyClosedItemTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
            closeTab()
        }.openTabDrawer {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.recently_closed_list), 1),
            ) {
                verifyRecentlyClosedTabsMenuView()
            }
        }.clickRecentlyClosedItem("Test_Page_1") {
            verifyUrl(website.url.toString())
        }
    }

    // Verifies that tapping the "x" button removes a recently closed item from the list
    @Test
    fun deleteRecentlyClosedTabsItemTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
            closeTab()
        }.openTabDrawer {
        }.openRecentlyClosedTabs {
            waitForListToExist()
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.recently_closed_list), 1),
            ) {
                verifyRecentlyClosedTabsMenuView()
            }
            clickDeleteRecentlyClosedTabs()
            verifyEmptyRecentlyClosedTabsList()
        }
    }

    // Verifies that deleting a Bookmarks folder also removes the item from inside it.
    @Test
    fun deleteNonEmptyBookmarkFolderTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(website.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarkTitle("Test_Page_1")
            createFolder("My Folder")
            verifyFolderTitle("My Folder")
        }.openThreeDotMenu("Test_Page_1") {
        }.clickEdit {
            clickParentFolderSelector()
            selectFolder("My Folder")
            navigateUp()
            saveEditBookmark()
            createFolder("My Folder 2")
            verifyFolderTitle("My Folder 2")
        }.openThreeDotMenu("My Folder 2") {
        }.clickEdit {
            clickParentFolderSelector()
            selectFolder("My Folder")
            navigateUp()
            saveEditBookmark()
        }.openThreeDotMenu("My Folder") {
        }.clickDelete {
            cancelFolderDeletion()
            verifyFolderTitle("My Folder")
        }.openThreeDotMenu("My Folder") {
        }.clickDelete {
            confirmDeletion()
            verifyDeleteSnackBarText()
            verifyBookmarkIsDeleted("My Folder")
            verifyBookmarkIsDeleted("My Folder 2")
            verifyBookmarkIsDeleted("Test_Page_1")
            navigateUp()
        }

        browserScreen {
        }.openThreeDotMenu {
            verifyAddBookmarkButton()
        }
    }

    @Test
    @Ignore("Failing after compose migration. See: https://github.com/mozilla-mobile/fenix/issues/26087")
    fun shareTabsFromTabsTrayTest() {
        val firstWebsite = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebsite = TestAssetHelper.getGenericAsset(mockWebServer, 2)
        val firstWebsiteTitle = firstWebsite.title
        val secondWebsiteTitle = secondWebsite.title
        val sharingApp = "Gmail"
        val sharedUrlsString = "${firstWebsite.url}\n\n${secondWebsite.url}"

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebsite.url) {
            verifyPageContent(firstWebsite.content)
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebsite.url.toString()) {
            verifyPageContent(secondWebsite.content)
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            verifyExistingOpenTabs("Test_Page_2")
        }.openTabsListThreeDotMenu {
            verifyShareAllTabsButton()
        }.clickShareAllTabsButton {
            verifyShareTabsOverlay(firstWebsiteTitle, secondWebsiteTitle)
            verifySharingWithSelectedApp(
                sharingApp,
                sharedUrlsString,
                "$firstWebsiteTitle, $secondWebsiteTitle",
            )
        }
    }

    @Test
    fun emptyTabsTrayViewPrivateBrowsingTest() {
        navigationToolbar {
        }.openTabTray {
        }.toggleToPrivateTabs() {
            verifyNormalBrowsingButtonIsSelected(false)
            verifyPrivateBrowsingButtonIsSelected(true)
            verifySyncedTabsButtonIsSelected(false)
            verifyNoOpenTabsInPrivateBrowsing()
            verifyPrivateBrowsingNewTabButton()
            verifyTabTrayOverflowMenu(true)
            verifyEmptyTabsTrayMenuButtons()
        }
    }

    @Test
    @Ignore("Failing after compose migration. See: https://github.com/mozilla-mobile/fenix/issues/26087")
    fun privateTabsTrayWithOpenedTabTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.togglePrivateBrowsingMode()

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
            verifyNormalBrowsingButtonIsSelected(false)
            verifyPrivateBrowsingButtonIsSelected(true)
            verifySyncedTabsButtonIsSelected(false)
            verifyTabTrayOverflowMenu(true)
            verifyTabsTrayCounter()
            verifyExistingTabList()
            verifyExistingOpenTabs(website.title)
            verifyCloseTabsButton(website.title)
            verifyOpenedTabThumbnail()
            verifyPrivateBrowsingNewTabButton()
        }
    }

    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    @Test
    fun noHistoryInPrivateBrowsingTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.togglePrivateBrowsingMode()

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openHistory {
            verifyEmptyHistoryView()
        }
    }

    @Test
    fun addPrivateBrowsingShortcutTest() {
        homeScreen {
        }.dismissOnboarding()

        homeScreen {
        }.triggerPrivateBrowsingShortcutPrompt {
            verifyNoThanksPrivateBrowsingShortcutButton()
            verifyAddPrivateBrowsingShortcutButton()
            clickAddPrivateBrowsingShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut("Private $appName") {}
        searchScreen {
            verifySearchView()
        }.dismissSearchBar {
            verifyPrivateSessionMessage()
        }
    }

    @Test
    fun mainMenuInstallPWATest() {
        val pwaPage = "https://mozilla-mobile.github.io/testapp/"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pwaPage.toUri()) {
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut("TEST_APP") {
            mDevice.waitForIdle()
            verifyNavURLBarHidden()
        }
    }

    // Verifies that reader mode is detected and the custom appearance controls are displayed
    @Test
    fun verifyReaderViewAppearanceUI() {
        val readerViewPage =
            TestAssetHelper.getLoremIpsumAsset(mockWebServer)
        val estimatedReadingTime = "1 - 2 minutes"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(readerViewPage.url) {
            mDevice.waitForIdle()
        }

        registerAndCleanupIdlingResources(
            ViewVisibilityIdlingResource(
                activityTestRule.activity.findViewById(R.id.mozac_browser_toolbar_page_actions),
                View.VISIBLE,
            ),
        ) {}

        navigationToolbar {
            verifyReaderViewDetected(true)
            toggleReaderView()
            mDevice.waitForIdle()
        }

        browserScreen {
            verifyPageContent(estimatedReadingTime)
        }.openThreeDotMenu {
            verifyReaderViewAppearance(true)
        }.openReaderViewAppearance {
            verifyAppearanceFontGroup(true)
            verifyAppearanceFontSansSerif(true)
            verifyAppearanceFontSerif(true)
            verifyAppearanceFontIncrease(true)
            verifyAppearanceFontDecrease(true)
            verifyAppearanceColorGroup(true)
            verifyAppearanceColorDark(true)
            verifyAppearanceColorLight(true)
            verifyAppearanceColorSepia(true)
        }
    }

    // Verifies the main menu of a custom tab with a custom menu item
    @Test
    fun customTabMenuItemsTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenu {
            verifyPoweredByTextIsDisplayed()
            verifyCustomMenuItem(customMenuItem)
            verifyDesktopSiteButtonExists()
            verifyFindInPageButtonExists()
            verifyOpenInBrowserButtonExists()
            verifyBackButtonExists()
            verifyForwardButtonExists()
            verifyRefreshButtonExists()
        }
    }

    // The test opens a link in a custom tab then sends it to the browser
    @Test
    fun openCustomTabInBrowserTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenu {
        }.clickOpenInBrowserButton {
            verifyTabCounter("1")
        }
    }

    @Test
    fun audioPlaybackSystemNotificationTest() {
        val audioTestPage = TestAssetHelper.getAudioPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(audioTestPage.url) {
            mDevice.waitForIdle()
            clickMediaPlayerPlayButton()
            assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
        }.openNotificationShade {
            verifySystemNotificationExists(audioTestPage.title)
            clickMediaNotificationControlButton("Pause")
            verifyMediaSystemNotificationButtonState("Play")
        }

        mDevice.pressBack()

        browserScreen {
            assertPlaybackState(browserStore, MediaSession.PlaybackState.PAUSED)
        }.openTabDrawer {
            closeTab()
        }

        mDevice.openNotification()

        notificationShade {
            verifySystemNotificationGone(audioTestPage.title)
        }

        // close notification shade before the next test
        mDevice.pressBack()
    }

    @Test
    @Ignore("Failing after compose migration. See: https://github.com/mozilla-mobile/fenix/issues/26087")
    fun tabMediaControlButtonTest() {
        val audioTestPage = TestAssetHelper.getAudioPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(audioTestPage.url) {
            mDevice.waitForIdle()
            clickMediaPlayerPlayButton()
            assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
        }.openTabDrawer {
            verifyTabMediaControlButtonState("Pause")
            clickTabMediaControlButton("Pause")
            verifyTabMediaControlButtonState("Play")
        }.openTab(audioTestPage.title) {
            assertPlaybackState(browserStore, MediaSession.PlaybackState.PAUSED)
        }
    }

    // For API>23
    // Verifies the default browser switch opens the system default apps menu.
    @Test
    fun changeDefaultBrowserSetting() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifyDefaultBrowserIsDisabled()
            clickDefaultBrowserSwitch()
            verifyAndroidDefaultAppsMenuAppears()
        }
        // Dismiss the request
        mDevice.pressBack()
    }

    @Test
    fun copyTextTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            longClickAndCopyText("content")
        }.openNavigationToolbar {
            openEditURLView()
        }

        searchScreen {
            clickClearButton()
            longClickToolbar()
            clickPasteText()
            verifyTypedToolbarText("content")
        }
    }

    @Test
    fun selectAllAndCopyTextTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            longClickAndCopyText("content", true)
        }.openNavigationToolbar {
            openEditURLView()
        }

        searchScreen {
            clickClearButton()
            longClickToolbar()
            clickPasteText()
            // with Select all, some white space is copied over, so we need to include that too
            verifyTypedToolbarText("  Page content: 1 ")
        }
    }

    @Test
    fun goToHomeScreenBottomToolbarTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            mDevice.waitForIdle()
        }.goToHomescreen {
            verifyHomeScreen()
        }
    }

    @Test
    fun goToHomeScreenTopToolbarTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openCustomizeSubMenu {
            clickTopToolbarToggle()
        }.goBack {
        }.goBack {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            mDevice.waitForIdle()
        }.goToHomescreen {
            verifyHomeScreen()
        }
    }

    @Test
    fun goToHomeScreenBottomToolbarPrivateModeTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            mDevice.waitForIdle()
        }.goToHomescreen {
            verifyHomeScreen()
        }
    }

    @Test
    fun goToHomeScreenTopToolbarPrivateModeTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }.openThreeDotMenu {
        }.openSettings {
        }.openCustomizeSubMenu {
            clickTopToolbarToggle()
        }.goBack {
        }.goBack {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            mDevice.waitForIdle()
        }.goToHomescreen {
            verifyHomeScreen()
        }
    }

    @Test
    fun tabsSettingsMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openTabsSubMenu {
            verifyTabViewOptions()
            verifyCloseTabsOptions()
            verifyMoveOldTabsToInactiveOptions()
        }
    }
}
