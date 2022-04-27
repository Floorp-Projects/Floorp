/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui

import android.view.View
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import androidx.test.espresso.IdlingRegistry
import androidx.test.filters.SdkSuppress
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
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.FeatureSettingsHelper
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.helpers.TestHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.ViewVisibilityIdlingResource
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.collectionRobot
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.enhancedTrackingProtection
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade
import org.mozilla.fenix.ui.robots.openEditURLView
import org.mozilla.fenix.ui.robots.searchScreen
import org.mozilla.fenix.ui.robots.tabDrawer
import org.mozilla.fenix.ui.util.FRENCH_LANGUAGE_HEADER
import org.mozilla.fenix.ui.util.FRENCH_SYSTEM_LOCALE_OPTION
import org.mozilla.fenix.ui.util.ROMANIAN_LANGUAGE_HEADER
import org.mozilla.fenix.ui.util.STRING_ONBOARDING_TRACKING_PROTECTION_HEADER

/**
 * Test Suite that contains a part of the Smoke and Sanity tests defined in TestRail:
 * https://testrail.stage.mozaws.net/index.php?/suites/view/3192
 * Other smoke tests have been marked with the @SmokeTest annotation throughout the ui package in order to limit this class expansion.
 * These tests will verify different functionalities of the app as a way to quickly detect regressions in main areas
 */
@Suppress("ForbiddenComment")
@SmokeTest
class SmokeTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer
    private var awesomeBar: ViewVisibilityIdlingResource? = null
    private var addonsListIdlingResource: RecyclerViewIdlingResource? = null
    private var recentlyClosedTabsListIdlingResource: RecyclerViewIdlingResource? = null
    private var readerViewNotification: ViewVisibilityIdlingResource? = null
    private val collectionName = "First Collection"
    private var bookmarksListIdlingResource: RecyclerViewIdlingResource? = null
    private var localeListIdlingResource: RecyclerViewIdlingResource? = null
    private val customMenuItem = "TestMenuItem"
    private lateinit var browserStore: BrowserStore
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityIntentTestRule(),
        { it.activity }
    )

    @get: Rule
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java, true, false
    )

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        // Initializing this as part of class construction, below the rule would throw a NPE
        // So we are initializing this here instead of in all related tests.
        browserStore = activityTestRule.activity.components.core.store

        // disabling the new homepage pop-up that interferes with the tests.
        featureSettingsHelper.setJumpBackCFREnabled(false)

        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()

        if (awesomeBar != null) {
            IdlingRegistry.getInstance().unregister(awesomeBar!!)
        }

        if (addonsListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(addonsListIdlingResource!!)
        }

        if (recentlyClosedTabsListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(recentlyClosedTabsListIdlingResource!!)
        }

        if (bookmarksListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
        }

        if (readerViewNotification != null) {
            IdlingRegistry.getInstance().unregister(readerViewNotification)
        }

        if (localeListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(localeListIdlingResource)
        }

        // resetting modified features enabled setting to default
        featureSettingsHelper.resetAllFeatureFlags()
    }

    // Verifies the first run onboarding screen
    @Test
    fun firstRunScreenTest() {
        homeScreen {
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenu()
            verifyHomeWordmark()

            verifyWelcomeHeader()
            // Sign in to Firefox
            verifyStartSyncHeader()
            verifyAccountsSignInButton()

            // Always-on privacy
            scrollToElementByText(STRING_ONBOARDING_TRACKING_PROTECTION_HEADER)
            verifyAutomaticPrivacyHeader()
            verifyAutomaticPrivacyText()

            // Choose your theme
            verifyChooseThemeHeader()
            verifyChooseThemeText()
            verifyDarkThemeDescription()
            verifyDarkThemeToggle()
            verifyLightThemeDescription()
            verifyLightThemeToggle()

            // Pick your toolbar placement
            verifyTakePositionHeader()
            verifyTakePositionElements()

            // Your privacy
            verifyYourPrivacyHeader()
            verifyYourPrivacyText()
            verifyPrivacyNoticeButton()

            // Start Browsing
            verifyStartBrowsingButton()
        }
    }

    @Test
    // Verifies the functionality of the onboarding Start Browsing button
    fun startBrowsingButtonTest() {
        homeScreen {
            verifyStartBrowsingButton()
        }.clickStartBrowsingButton {
            verifySearchView()
        }
    }

    @Test
    /* Verifies the nav bar:
     - opening a web page
     - the existence of nav bar items
     - editing the url bar
     - the tab drawer button
     - opening a new search and dismissing the nav bar
    */
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

    @Test
    // Verifies the list of items in a tab's 3 dot menu
    fun verifyPageMainMenuItemsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            verifyPageThreeDotMainMenuItems()
        }
    }

    // Could be removed when more smoke tests from the History category are added
    @Test
    // Verifies the History menu opens from a tab's 3 dot menu
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
    @Test
    // Verifies the Bookmarks menu opens from a tab's 3 dot menu
    fun openMainMenuBookmarksItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarksMenuView()
        }
    }

    @Test
    // Verifies the Add-ons menu opens from a tab's 3 dot menu
    fun openMainMenuAddonsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            addonsListIdlingResource =
                RecyclerViewIdlingResource(
                    activityTestRule.activity.findViewById(R.id.add_ons_list),
                    1
                )
            IdlingRegistry.getInstance().register(addonsListIdlingResource!!)
            verifyAddonsItems()
            IdlingRegistry.getInstance().unregister(addonsListIdlingResource!!)
        }
    }

    @Test
    // Verifies the Synced tabs menu or Sync Sign In menu opens from a tab's 3 dot menu.
    // The test is assuming we are NOT signed in.
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

    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Verifies the Settings menu opens from a tab's 3 dot menu
    fun openMainMenuSettingsItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }
    }

    @Test
    // Verifies the Find in page option in a tab's 3 dot menu
    fun openMainMenuFindInPageTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openFindInPage {
            verifyFindInPageSearchBarItems()
        }
    }

    @Test
    // Verifies the Add to home screen option in a tab's 3 dot menu
    fun mainMenuAddToHomeScreenTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

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
            addShortcutName("Test Page")
            clickAddShortcutButton()
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut("Test Page") {
            verifyUrl(website.url.toString())
            verifyTabCounter("1")
        }
    }

    @Test
    // Verifies the Add to collection option in a tab's 3 dot menu
    fun openMainMenuAddToCollectionTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openSaveToCollection {
            verifyCollectionNameTextField()
        }
    }

    @Test
    // Verifies the Bookmark button in a tab's 3 dot menu
    fun mainMenuBookmarkButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.bookmarkPage {
            verifySnackBarText("Bookmark saved!")
        }
    }

    @Test
    // Device or AVD requires a Google Services Android OS installation with Play Store installed
    // Verifies the Open in app button when an app is installed
    fun mainMenuOpenInAppTest() {
        val playStoreUrl = "play.google.com/store/apps/details?id=org.mozilla.fenix"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(playStoreUrl.toUri()) {
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickOpenInApp {
            assertNativeAppOpens(Constants.PackageName.GOOGLE_PLAY_SERVICES, playStoreUrl)
        }
    }

    @Test
    // Verifies the Desktop site toggle in a tab's 3 dot menu
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

    @Test
    // Verifies the Share button in a tab's 3 dot menu
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

    @Test
    // Verifies the refresh button in a tab's 3 dot menu
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

    @Test
    fun customTrackingProtectionSettingsTest() {
        val genericWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val trackingPage = TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            verifyEnhancedTrackingProtectionOptionsEnabled()
            selectTrackingProtectionOption("Custom")
            verifyCustomTrackingProtectionSettings()
        }.goBackToHomeScreen {}

        navigationToolbar {
            // browsing a basic page to allow GV to load on a fresh run
        }.enterURLAndEnterToBrowser(genericWebPage.url) {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(trackingPage.url) {}

        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
        }.openDetails {
            verifyTrackingCookiesBlocked()
            verifyCryptominersBlocked()
            verifyFingerprintersBlocked()
            verifyTrackingContentBlocked()
            viewTrackingContentBlockList()
        }
    }

    @Test
    // Verifies changing the default engine from the Search Shortcut menu
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

    @Test
    // Swipes the nav bar left/right to switch between tabs
    fun swipeToSwitchTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            swipeNavBarRight(secondWebPage.url.toString())
            verifyUrl(firstWebPage.url.toString())
            swipeNavBarLeft(firstWebPage.url.toString())
            verifyUrl(secondWebPage.url.toString())
        }
    }

    @Test
    // Saves a login, then changes it and verifies the update
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

    @Test
    @SdkSuppress(minSdkVersion = 29)
    // Verifies that you can go to System settings and change app's permissions from inside the app
    fun redirectToAppPermissionsSystemSettingsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openCamera {
            verifyBlockedByAndroid()
        }.goBack {
        }.openLocation {
            verifyBlockedByAndroid()
        }.goBack {
        }.openMicrophone {
            verifyBlockedByAndroid()
            clickGoToSettingsButton()
            openAppSystemPermissionsSettings()
            switchAppPermissionSystemSetting("Camera", "Allow")
            mDevice.pressBack()
            switchAppPermissionSystemSetting("Location", "Allow")
            mDevice.pressBack()
            switchAppPermissionSystemSetting("Microphone", "Allow")
            mDevice.pressBack()
            mDevice.pressBack()
            mDevice.pressBack()
            verifyUnblockedByAndroid()
        }.goBack {
        }.openLocation {
            verifyUnblockedByAndroid()
        }.goBack {
        }.openCamera {
            verifyUnblockedByAndroid()
        }
    }

    @Test
    // Verifies that a recently closed item is properly opened
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
            recentlyClosedTabsListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.recently_closed_list), 1)
            IdlingRegistry.getInstance().register(recentlyClosedTabsListIdlingResource!!)
            verifyRecentlyClosedTabsMenuView()
            IdlingRegistry.getInstance().unregister(recentlyClosedTabsListIdlingResource!!)
        }.clickRecentlyClosedItem("Test_Page_1") {
            verifyUrl(website.url.toString())
        }
    }

    @Test
    // Verifies that tapping the "x" button removes a recently closed item from the list
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
            recentlyClosedTabsListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.recently_closed_list), 1)
            IdlingRegistry.getInstance().register(recentlyClosedTabsListIdlingResource!!)
            verifyRecentlyClosedTabsMenuView()
            IdlingRegistry.getInstance().unregister(recentlyClosedTabsListIdlingResource!!)
            clickDeleteRecentlyClosedTabs()
            verifyEmptyRecentlyClosedTabsList()
        }
    }

    @Test
    fun createFirstCollectionTest() {
        // disabling these features to have better visibility of Collections
        featureSettingsHelper.setRecentTabsFeatureEnabled(false)
        featureSettingsHelper.setPocketEnabled(false)

        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            mDevice.waitForIdle()
        }.goToHomescreen {
            swipeToBottom()
        }.clickSaveTabsToCollectionButton {
            longClickTab(firstWebPage.title)
            selectTab(secondWebPage.title)
        }.clickSaveCollection {
            typeCollectionNameAndSave(collectionName)
        }

        tabDrawer {
            verifySnackBarText("Collection saved!")
            snackBarButtonClick("VIEW")
        }

        homeScreen {
            verifyCollectionIsDisplayed(collectionName)
            verifyCollectionIcon()
        }
    }

    @Test
    fun verifyExpandedCollectionItemsTest() {
        // disabling these features to have better visibility of Collections
        featureSettingsHelper.setRecentTabsFeatureEnabled(false)
        featureSettingsHelper.setPocketEnabled(false)

        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName)
            snackBarButtonClick("VIEW")
        }

        homeScreen {
            verifyCollectionIsDisplayed(collectionName)
            verifyCollectionIcon()
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(webPage.title)
            verifyCollectionTabLogo(true)
            verifyCollectionTabUrl(true)
            verifyShareCollectionButtonIsVisible(true)
            verifyCollectionMenuIsVisible(true)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, true)
        }.collapseCollection(collectionName) {}

        collectionRobot {
            verifyTabSavedInCollection(webPage.title, false)
            verifyShareCollectionButtonIsVisible(false)
            verifyCollectionMenuIsVisible(false)
            verifyCollectionTabLogo(false)
            verifyCollectionTabUrl(false)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, false)
        }

        homeScreen {
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(webPage.title)
            verifyCollectionTabLogo(true)
            verifyCollectionTabUrl(true)
            verifyShareCollectionButtonIsVisible(true)
            verifyCollectionMenuIsVisible(true)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, true)
        }.collapseCollection(collectionName) {}

        collectionRobot {
            verifyTabSavedInCollection(webPage.title, false)
            verifyShareCollectionButtonIsVisible(false)
            verifyCollectionMenuIsVisible(false)
            verifyCollectionTabLogo(false)
            verifyCollectionTabUrl(false)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, false)
        }
    }

    @Test
    fun openAllTabsInCollectionTest() {
        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName)
            verifySnackBarText("Collection saved!")
            closeTab()
        }

        homeScreen {
        }.expandCollection(collectionName) {
            clickCollectionThreeDotButton()
            selectOpenTabs()
        }
        tabDrawer {
            verifyExistingOpenTabs(webPage.title)
        }
    }

    @Test
    fun shareCollectionTest() {
        val firstWebsite = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebsite = TestAssetHelper.getGenericAsset(mockWebServer, 2)
        val sharingApp = "Gmail"
        val urlString = "${secondWebsite.url}\n\n${firstWebsite.url}"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebsite.url) {
            verifyPageContent(firstWebsite.content)
        }.openTabDrawer {
            createCollection(firstWebsite.title, collectionName)
        }.openNewTab {
        }.submitQuery(secondWebsite.url.toString()) {
            verifyPageContent(secondWebsite.content)
        }.openThreeDotMenu {
        }.openSaveToCollection {
        }.selectExistingCollection(collectionName) {
        }.goToHomescreen {
        }.expandCollection(collectionName) {
        }.clickShareCollectionButton {
            verifyShareTabsOverlay(firstWebsite.title, secondWebsite.title)
            selectAppToShareWith(sharingApp)
            verifySharedTabsIntent(urlString, collectionName)
        }
    }

    @Ignore("Failing, see: https://github.com/mozilla-mobile/fenix/issues/23296")
    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    fun deleteCollectionTest() {
        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName)
            snackBarButtonClick("VIEW")
        }

        homeScreen {
        }.expandCollection(collectionName) {
            clickCollectionThreeDotButton()
            selectDeleteCollection()
        }

        homeScreen {
            verifySnackBarText("Collection deleted")
            verifyNoCollectionsText()
        }
    }

    @Test
    // Verifies that deleting a Bookmarks folder also removes the item from inside it.
    fun deleteNonEmptyBookmarkFolderTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        browserScreen {
            createBookmark(website.url)
        }.openThreeDotMenu {
        }.openBookmarks {
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 1)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
            verifyBookmarkTitle("Test_Page_1")
            createFolder("My Folder")
            verifyFolderTitle("My Folder")
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
        }.openThreeDotMenu("Test_Page_1") {
        }.clickEdit {
            clickParentFolderSelector()
            selectFolder("My Folder")
            navigateUp()
            saveEditBookmark()
            createFolder("My Folder 2")
            bookmarksListIdlingResource =
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.bookmark_list), 1)
            IdlingRegistry.getInstance().register(bookmarksListIdlingResource!!)
            verifyFolderTitle("My Folder 2")
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
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
            selectAppToShareWith(sharingApp)
            verifySharedTabsIntent(
                sharedUrlsString,
                "$firstWebsiteTitle, $secondWebsiteTitle"
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
        }.openTab(website.title) {
            verifyUrl(website.url.toString())
            verifyTabCounter("1")
        }
    }

    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
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

    @Test
    // Verifies that reader mode is detected and the custom appearance controls are displayed
    fun verifyReaderViewAppearanceUI() {
        val readerViewPage =
            TestAssetHelper.getLoremIpsumAsset(mockWebServer)
        val estimatedReadingTime = "1 - 2 minutes"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(readerViewPage.url) {
            org.mozilla.fenix.ui.robots.mDevice.waitForIdle()
        }

        readerViewNotification = ViewVisibilityIdlingResource(
            activityTestRule.activity.findViewById(R.id.mozac_browser_toolbar_page_actions),
            View.VISIBLE
        )

        IdlingRegistry.getInstance().register(readerViewNotification)

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

    @Test
    // Verifies the main menu of a custom tab with a custom menu item
    fun customTabMenuItemsTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem
            )
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

    @Test
    // The test opens a link in a custom tab then sends it to the browser
    fun openCustomTabInBrowserTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString()
            )
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

    @Test
    // For API>23
    // Verifies the default browser switch opens the system default apps menu.
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
            verifyPastedToolbarText("content")
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
            verifyPastedToolbarText("Page content: 1")
        }
    }

    @Test
    fun switchLanguageTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openLanguageSubMenu {
            localeListIdlingResource =
                RecyclerViewIdlingResource(
                    activityTestRule.activity.findViewById(R.id.locale_list),
                    2
                )
            IdlingRegistry.getInstance().register(localeListIdlingResource)
            selectLanguage("Romanian")
            verifyLanguageHeaderIsTranslated(ROMANIAN_LANGUAGE_HEADER)
            selectLanguage("Français")
            verifyLanguageHeaderIsTranslated(FRENCH_LANGUAGE_HEADER)
            selectLanguage(FRENCH_SYSTEM_LOCALE_OPTION)
            verifyLanguageHeaderIsTranslated("Language")
            IdlingRegistry.getInstance().unregister(localeListIdlingResource)
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
