/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.view.View
import androidx.core.net.toUri
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.IdlingRegistry
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.rule.GrantPermissionRule
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
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.TestHelper.deleteDownloadFromStorage
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.ViewVisibilityIdlingResource
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickTabCrashedRestoreButton
import org.mozilla.fenix.ui.robots.clickUrlbar
import org.mozilla.fenix.ui.robots.collectionRobot
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.downloadRobot
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
 * Test Suite that contains tests defined as part of the Smoke and Sanity check defined in Test rail.
 * These tests will verify different functionalities of the app as a way to quickly detect regressions in main areas
 */
@Suppress("ForbiddenComment")
class SmokeTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer
    private var awesomeBar: ViewVisibilityIdlingResource? = null
    private var searchSuggestionsIdlingResource: RecyclerViewIdlingResource? = null
    private var addonsListIdlingResource: RecyclerViewIdlingResource? = null
    private var recentlyClosedTabsListIdlingResource: RecyclerViewIdlingResource? = null
    private var readerViewNotification: ViewVisibilityIdlingResource? = null
    private val downloadFileName = "Globe.svg"
    private val collectionName = "First Collection"
    private var bookmarksListIdlingResource: RecyclerViewIdlingResource? = null
    private var localeListIdlingResource: RecyclerViewIdlingResource? = null
    private val customMenuItem = "TestMenuItem"

    // This finds the dialog fragment child of the homeFragment, otherwise the awesomeBar would return null
    private fun getAwesomebarView(): View? {
        val homeFragment = activityTestRule.activity.supportFragmentManager.primaryNavigationFragment
        val searchDialogFragment = homeFragment?.childFragmentManager?.fragments?.first {
            it.javaClass.simpleName == "SearchDialogFragment"
        }
        return searchDialogFragment?.view?.findViewById(R.id.awesome_bar)
    }

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule()
    private lateinit var browserStore: BrowserStore

    @get: Rule
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java, true, false
    )

    @get:Rule
    var mGrantPermissions = GrantPermissionRule.grant(
        android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
        android.Manifest.permission.READ_EXTERNAL_STORAGE
    )

    @Before
    fun setUp() {
        // Initializing this as part of class construction, below the rule would throw a NPE
        // So we are initializing this here instead of in all related tests.
        browserStore = activityTestRule.activity.components.core.store

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

        if (searchSuggestionsIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(searchSuggestionsIdlingResource!!)
        }

        if (addonsListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(addonsListIdlingResource!!)
        }

        if (recentlyClosedTabsListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(recentlyClosedTabsListIdlingResource!!)
        }

        deleteDownloadFromStorage(downloadFileName)

        if (bookmarksListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(bookmarksListIdlingResource!!)
        }

        if (readerViewNotification != null) {
            IdlingRegistry.getInstance().unregister(readerViewNotification)
        }

        if (localeListIdlingResource != null) {
            IdlingRegistry.getInstance().unregister(localeListIdlingResource)
        }
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
        homeScreen {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryMenuView()
        }
    }

    // Could be removed when more smoke tests from the Bookmarks category are added
    @Test
    // Verifies the Bookmarks menu opens from a tab's 3 dot menu
    fun openMainMenuBookmarksItemTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarksMenuView()
        }
    }

    @Test
    // Verifies the Synced tabs menu or Sync Sign In menu opens from a tab's 3 dot menu.
    // The test is assuming we are NOT signed in.
    fun openMainMenuSyncItemTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSyncSignIn {
            verifySyncSignInMenuHeader()
        }
    }

    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Verifies the Settings menu opens from a tab's 3 dot menu
    fun openMainMenuSettingsItemTest() {
        homeScreen {
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
    // Verifies the Add to top sites option in a tab's 3 dot menu
    fun openMainMenuAddTopSiteTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            expandMenu()
        }.addToFirefoxHome {
            verifySnackBarText("Added to top sites!")
        }.openTabDrawer {
        }.openNewTab {
        }.dismissSearchBar {
            verifyExistingTopSitesTabs(defaultWebPage.title)
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
    // Verifies the Share button in a tab's 3 dot menu
    fun mainMenuShareButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.sharePage {
            verifyShareAppsLayout()
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
    @Ignore("https://github.com/mozilla-mobile/fenix/issues/20868")
    fun customTrackingProtectionSettingsTest() {
        val genericWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val trackingPage = TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            verifyEnhancedTrackingProtectionOptions()
            selectTrackingProtectionOption("Custom")
            verifyCustomTrackingProtectionSettings()
        }.goBackToHomeScreen {}

        navigationToolbar {
            // browsing a basic page to allow GV to load on a fresh run
        }.enterURLAndEnterToBrowser(genericWebPage.url) {
        }.openNavigationToolbar {
        }.openTrackingProtectionTestPage(trackingPage.url, true) {}

        enhancedTrackingProtection {
        }.openEnhancedTrackingProtectionSheet {
        }.openDetails {
            verifyTrackingCookiesBlocked()
            verifyCryptominersBlocked()
            verifyFingerprintersBlocked()
            verifyBasicLevelTrackingContentBlocked()
        }
    }

    @Test
    // Verifies changing the default engine from the Search Shortcut menu
    fun verifySearchEngineCanBeChangedTemporarilyUsingShortcuts() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openSearch {
            verifyKeyboardVisibility()
            clickSearchEngineShortcutButton()
            verifySearchEngineList()
            changeDefaultSearchEngine("Amazon.com")
            verifySearchEngineIcon("Amazon.com")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
        }.openNewTab {
            clickSearchEngineShortcutButton()
            mDevice.waitForIdle()
            changeDefaultSearchEngine("Bing")
            verifySearchEngineIcon("Bing")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
        }.openNewTab {
            clickSearchEngineShortcutButton()
            mDevice.waitForIdle()
            changeDefaultSearchEngine("DuckDuckGo")
            verifySearchEngineIcon("DuckDuckGo")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
        }.openNewTab {
            clickSearchEngineShortcutButton()
            changeDefaultSearchEngine("Wikipedia")
            verifySearchEngineIcon("Wikipedia")
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            // Checking whether the next search will be with default or not
        }.openNewTab {
        }.goToSearchEngine {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openNavigationToolbar {
            clickUrlbar {
                verifyDefaultSearchEngine("Google")
            }
        }
    }

    @Test
    // Ads a new search engine from the list of custom engines
    fun addPredefinedSearchEngineTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            openAddSearchEngineMenu()
            verifyAddSearchEngineList()
            addNewSearchEngine("YouTube")
            verifyEngineListContains("YouTube")
        }.goBack {
        }.goBack {
        }.openSearch {
            verifyKeyboardVisibility()
            clickSearchEngineShortcutButton()
            verifyEnginesListShortcutContains("YouTube")
        }
    }

    @Test
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    // Goes through the settings and changes the search suggestion toggle, then verifies it changes.
    fun toggleSearchSuggestions() {

        homeScreen {
        }.openNavigationToolbar {
            typeSearchTerm("mozilla")
            val awesomeBarView = getAwesomebarView()
            awesomeBarView?.let {
                awesomeBar = ViewVisibilityIdlingResource(it, View.VISIBLE)
            }
            IdlingRegistry.getInstance().register(awesomeBar!!)
            searchSuggestionsIdlingResource =
                RecyclerViewIdlingResource(awesomeBarView as RecyclerView, 1)
            IdlingRegistry.getInstance().register(searchSuggestionsIdlingResource!!)
            verifySearchSuggestionsAreMoreThan(0)
            IdlingRegistry.getInstance().unregister(searchSuggestionsIdlingResource!!)
        }.goBack {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            disableShowSearchSuggestions()
        }.goBack {
        }.goBack {
        }.openNavigationToolbar {
            typeSearchTerm("mozilla")
            searchSuggestionsIdlingResource =
                RecyclerViewIdlingResource(getAwesomebarView() as RecyclerView)
            IdlingRegistry.getInstance().register(searchSuggestionsIdlingResource!!)
            verifySearchSuggestionsAreEqualTo(0)
            IdlingRegistry.getInstance().unregister(searchSuggestionsIdlingResource!!)
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
    @Ignore("To be fixed in https://github.com/mozilla-mobile/fenix/issues/20702")
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
            verifySavedLoginFromPrompt()
            viewSavedLoginDetails()
            revealPassword()
            verifyPasswordSaved("test") // failing here locally
        }
    }

    @Test
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
            switchAppPermissionSystemSetting("Camera")
            switchAppPermissionSystemSetting("Location")
            switchAppPermissionSystemSetting("Microphone")
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
    // Installs uBlock add-on and checks that the app doesn't crash while loading pages with trackers
    fun noCrashWithAddonInstalledTest() {
        // setting ETP to Strict mode to test it works with add-ons
        activityTestRule.activity.settings().setStrictETP()

        val addonName = "uBlock Origin"
        val trackingProtectionPage =
            TestAssetHelper.getEnhancedTrackingProtectionAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            addonsListIdlingResource =
                RecyclerViewIdlingResource(
                    activityTestRule.activity.findViewById(R.id.add_ons_list),
                    1
                )
            IdlingRegistry.getInstance().register(addonsListIdlingResource!!)
            clickInstallAddon(addonName)
            acceptInstallAddon()
            verifyDownloadAddonPrompt(addonName, activityTestRule)
            IdlingRegistry.getInstance().unregister(addonsListIdlingResource!!)
        }.goBack {
        }.openNavigationToolbar {
        }.openTrackingProtectionTestPage(trackingProtectionPage.url, true) {}
    }

    @Test
    // This test verifies the Recently Closed Tabs List and items
    fun verifyRecentlyClosedTabsListTest() {
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
            verifyRecentlyClosedTabsPageTitle("Test_Page_1")
            verifyRecentlyClosedTabsUrl(website.url)
        }
    }

    @Test
    // Verifies the items from the overflow menu of Recently Closed Tabs
    fun recentlyClosedTabsMenuItemsTest() {
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
            openRecentlyClosedTabsThreeDotMenu()
            verifyRecentlyClosedTabsMenuCopy()
            verifyRecentlyClosedTabsMenuShare()
            verifyRecentlyClosedTabsMenuNewTab()
            verifyRecentlyClosedTabsMenuPrivateTab()
            verifyRecentlyClosedTabsMenuDelete()
        }
    }

    @Test
    // Verifies the Copy option from the Recently Closed Tabs overflow menu
    fun copyRecentlyClosedTabsItemTest() {
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
            openRecentlyClosedTabsThreeDotMenu()
            verifyRecentlyClosedTabsMenuCopy()
            clickCopyRecentlyClosedTabs()
            verifyCopyRecentlyClosedTabsSnackBarText()
        }
    }

    @Test
    // Verifies the Share option from the Recently Closed Tabs overflow menu
    fun shareRecentlyClosedTabsItemTest() {
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
            openRecentlyClosedTabsThreeDotMenu()
            verifyRecentlyClosedTabsMenuShare()
            clickShareRecentlyClosedTabs()
            verifyShareOverlay()
            verifyShareTabTitle("Test_Page_1")
            verifyShareTabUrl(website.url)
            verifyShareTabFavicon()
        }
    }

    @Test
    // Verifies the Open in a new tab option from the Recently Closed Tabs overflow menu
    fun openRecentlyClosedTabsInNewTabTest() {
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
            openRecentlyClosedTabsThreeDotMenu()
            verifyRecentlyClosedTabsMenuNewTab()
        }.clickOpenInNewTab {
            verifyUrl(website.url.toString())
        }.openTabDrawer {
            verifyNormalModeSelected()
        }
    }

    @Test
    // Verifies the Open in a private tab option from the Recently Closed Tabs overflow menu
    fun openRecentlyClosedTabsInNewPrivateTabTest() {
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
            openRecentlyClosedTabsThreeDotMenu()
            verifyRecentlyClosedTabsMenuPrivateTab()
        }.clickOpenInPrivateTab {
            verifyUrl(website.url.toString())
        }.openTabDrawer {
            verifyPrivateModeSelected()
        }
    }

    @Test
    // Verifies the delete option from the Recently Closed Tabs overflow menu
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
            openRecentlyClosedTabsThreeDotMenu()
            verifyRecentlyClosedTabsMenuDelete()
            clickDeleteCopyRecentlyClosedTabs()
            verifyEmptyRecentlyClosedTabsList()
        }
    }

    @Test
    /* Verifies downloads in the Downloads Menu:
      - downloads appear in the list
      - deleting a download from device storage, removes it from the Downloads Menu too
    */
    fun manageDownloadsInDownloadsMenuTest() {
        val downloadWebPage = TestAssetHelper.getDownloadAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(downloadWebPage.url) {
            mDevice.waitForIdle()
        }

        downloadRobot {
            verifyDownloadPrompt()
        }.clickDownload {
            mDevice.waitForIdle()
            verifyDownloadNotificationPopup()
        }

        browserScreen {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            waitForDownloadsListToExist()
            verifyDownloadedFileName(downloadFileName)
            verifyDownloadedFileIcon()
            deleteDownloadFromStorage(downloadFileName)
        }.exitDownloadsManagerToBrowser {
        }.openThreeDotMenu {
        }.openDownloadsManager {
            verifyEmptyDownloadsList()
        }
    }

    @Test
    fun createFirstCollectionTest() {
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
            verifyCollectionTabLogo()
            verifyCollectionTabUrl()
            verifyShareCollectionButtonIsVisible(true)
            verifyCollectionMenuIsVisible(true)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, true)
        }.collapseCollection(collectionName) {}

        collectionRobot {
            verifyTabSavedInCollection(webPage.title, false)
            verifyShareCollectionButtonIsVisible(false)
            verifyCollectionMenuIsVisible(false)
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
        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName)
            snackBarButtonClick("VIEW")
        }

        homeScreen {
        }.expandCollection(collectionName) {
            clickShareCollectionButton()
        }

        homeScreen {
            verifyShareTabsOverlay()
        }
    }

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
        }.openThreeDotMenu("My Folder") {
        }.clickDelete {
            cancelFolderDeletion()
            verifyFolderTitle("My Folder")
        }.openThreeDotMenu("My Folder") {
        }.clickDelete {
            confirmFolderDeletion()
            verifyDeleteSnackBarText()
            navigateUp()
        }

        browserScreen {
        }.openThreeDotMenu {
            verifyBookmarksButton()
        }
    }

    @Test
    fun shareTabsFromTabsTrayTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
            verifyNormalModeSelected()
            verifyExistingTabList()
            verifyExistingOpenTabs("Test_Page_1")
            verifyTabTrayOverflowMenu(true)
        }.openTabsListThreeDotMenu {
            verifyShareAllTabsButton()
            clickShareAllTabsButton()
            verifyShareTabsOverlay()
        }
    }

    @Test
    fun emptyTabsTrayViewPrivateBrowsingTest() {
        homeScreen {
        }.dismissOnboarding()

        homeScreen {
        }.openTabDrawer {
        }.toggleToPrivateTabs() {
            verifyPrivateModeSelected()
            verifyNormalBrowsingButtonIsDisplayed()
            verifyNoTabsOpened()
            verifyTabTrayOverflowMenu(true)
            verifyNewTabButton()
        }.openTabsListThreeDotMenu {
            verifyTabSettingsButton()
            verifyRecentlyClosedTabsButton()
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
            verifyPrivateModeSelected()
            verifyNormalBrowsingButtonIsDisplayed()
            verifyExistingTabList()
            verifyExistingOpenTabs("Test_Page_1")
            verifyCloseTabsButton("Test_Page_1")
            verifyOpenedTabThumbnail()
            verifyTabTrayOverflowMenu(true)
            verifyNewTabButton()
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
    @Ignore("To be re-enabled later. See https://github.com/mozilla-mobile/fenix/issues/20716")
    fun mainMenuInstallPWATest() {
        val pwaPage = "https://mozilla-mobile.github.io/testapp/"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(pwaPage.toUri()) {
            verifyNotificationDotOnMainMenu()
        }.openThreeDotMenu {
        }.clickInstall {
            clickAddAutomaticallyButton()
        }.openHomeScreenShortcut("yay app") {
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
    fun closeTabCrashedReporterTest() {

        homeScreen {
        }.openNavigationToolbar {
        }.openTabCrashReporter {
        }.clickTabCrashedCloseButton {
        }.openTabDrawer {
            verifyNoTabsOpened()
        }
    }

    @Ignore("Test failure caused by: https://github.com/mozilla-mobile/fenix/issues/19964")
    @Test
    fun restoreTabCrashedReporterTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {}

        navigationToolbar {
        }.openTabCrashReporter {
            clickTabCrashedRestoreButton()
            verifyPageContent(website.content)
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
            clickMediaSystemNotificationControlButton("Pause")
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
            clickTabMediaControlButton()
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
            verifyDefaultBrowserIsDisaled()
            clickDefaultBrowserSwitch()
            verifyAndroidDefaultAppsMenuAppears()
        }
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
    fun startOnHomeSettingsMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openTabsSubMenu {
            verifyStartOnHomeOptions()
        }
    }

    @Test
    fun alwaysStartOnHomeTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSettings {
        }.openTabsSubMenu {
            clickAlwaysStartOnHomeToggle()
        }

        restartApp(activityTestRule)

        homeScreen {
            verifyHomeScreen()
        }
    }
}
