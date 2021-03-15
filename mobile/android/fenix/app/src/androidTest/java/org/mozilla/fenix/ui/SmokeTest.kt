/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.view.View
import androidx.core.net.toUri
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.IdlingRegistry
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.GrantPermissionRule
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.deleteDownloadFromStorage
import org.mozilla.fenix.helpers.ViewVisibilityIdlingResource
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickTabCrashedRestoreButton
import org.mozilla.fenix.ui.robots.clickUrlbar
import org.mozilla.fenix.ui.robots.dismissTrackingOnboarding
import org.mozilla.fenix.ui.robots.downloadRobot
import org.mozilla.fenix.ui.robots.enhancedTrackingProtection
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.tabDrawer

/**
 * Test Suite that contains tests defined as part of the Smoke and Sanity check defined in Test rail.
 * These tests will verify different functionalities of the app as a way to quickly detect regressions in main areas
 */

class SmokeTest {
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer
    private var awesomeBar: ViewVisibilityIdlingResource? = null
    private var searchSuggestionsIdlingResource: RecyclerViewIdlingResource? = null
    private var addonsListIdlingResource: RecyclerViewIdlingResource? = null
    private var recentlyClosedTabsListIdlingResource: RecyclerViewIdlingResource? = null
    private var readerViewNotification: ViewVisibilityIdlingResource? = null
    private val downloadFileName = "Globe.svg"
    val collectionName = "First Collection"
    private var bookmarksListIdlingResource: RecyclerViewIdlingResource? = null

    // This finds the dialog fragment child of the homeFragment, otherwise the awesomeBar would return null
    private fun getAwesomebarView(): View? {
        val homeFragment = activityTestRule.activity.supportFragmentManager.primaryNavigationFragment
        val searchDialogFragment = homeFragment?.childFragmentManager?.fragments?.first {
            it.javaClass.simpleName == "SearchDialogFragment"
        }
        return searchDialogFragment?.view?.findViewById(R.id.awesome_bar)
    }

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @get:Rule
    var mGrantPermissions = GrantPermissionRule.grant(
        android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
        android.Manifest.permission.READ_EXTERNAL_STORAGE
    )

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
    }

    // copied over from HomeScreenTest
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

            // Intro to other sections
            verifyGetToKnowHeader()

            // Automatic privacy
            scrollToElementByText("Automatic privacy")
            verifyAutomaticPrivacyHeader()
            verifyTrackingProtectionToggle()
            verifyAutomaticPrivacyText()

            // Choose your theme
            verifyChooseThemeHeader()
            verifyChooseThemeText()
            verifyDarkThemeDescription()
            verifyDarkThemeToggle()
            verifyLightThemeDescription()
            verifyLightThemeToggle()

            // Browse privately
            verifyBrowsePrivatelyHeader()
            verifyBrowsePrivatelyText()

            // Take a position
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
    @Ignore("To be re-implemented with the three dot menu changes https://github.com/mozilla-mobile/fenix/issues/17870")
    fun verifyPageMainMenuItemsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
            verifyThreeDotMainMenuItems()
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
    // Verifies the Synced tabs menu opens from a tab's 3 dot menu
    fun openMainMenuSyncedTabsItemTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSyncedTabs {
            verifySyncedTabsMenuHeader()
        }
    }

    // Could be removed when more smoke tests from the Settings category are added
    @Test
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
        }.openAddToHomeScreen {
            clickCancelShortcutButton()
        }

        browserScreen {
        }.openThreeDotMenu {
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
    @Ignore("To be re-implemented in https://github.com/mozilla-mobile/fenix/issues/17979")
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
            verifyRefreshButton()
        }.refreshPage {
            verifyPageContent("REFRESHED")
        }
    }

    @Test
    // Turns ETP toggle off from Settings and verifies the ETP shield is not displayed in the nav bar
    @Ignore("To be re-implemented with the three dot menu changes https://github.com/mozilla-mobile/fenix/issues/17870")
    fun verifyETPShieldNotDisplayedIfOFFGlobally() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openEnhancedTrackingProtectionSubMenu {
            switchEnhancedTrackingProtectionToggle()
            verifyEnhancedTrackingProtectionOptionsGrayedOut()
        }.goBackToHomeScreen {
            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
                verifyEnhancedTrackingProtectionPanelNotVisible()
            }.openThreeDotMenu {
            }.openSettings {
            }.openEnhancedTrackingProtectionSubMenu {
                switchEnhancedTrackingProtectionToggle()
            }.goBack {
            }.goBackToBrowser {
                clickEnhancedTrackingProtectionPanel()
                verifyEnhancedTrackingProtectionSwitch()
                clickEnhancedTrackingProtectionSwitchOffOn()
            }
        }
    }

    @Test
    fun customTrackingProtectionSettingsTest() {
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
        }.openTrackingProtectionTestPage(trackingPage.url, true) {}

        enhancedTrackingProtection {
            dismissTrackingOnboarding()
        }.openEnhancedTrackingProtectionSheet {
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
    @Ignore("To be re-implemented with the three dot menu changes https://github.com/mozilla-mobile/fenix/issues/17870")
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
            verifyPasswordSaved("test")
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
    @Ignore("To be re-implemented in https://github.com/mozilla-mobile/fenix/issues/17799")
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
        enhancedTrackingProtection {
            verifyEnhancedTrackingProtectionNotice()
        }.closeNotificationPopup {}

        browserScreen {
        }.openThreeDotMenu {
        }.openReportSiteIssue {
            verifyUrl("webcompat.com/issues/new")
        }
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
            selectTab(firstWebPage.title)
            selectTab(secondWebPage.title)
            clickSaveCollection()
            typeCollectionName(collectionName)
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
            expandCollection(collectionName)
            verifyTabSavedInCollection(webPage.title)
            verifyCollectionTabLogo()
            verifyCollectionTabUrl()
            verifyShareCollectionButtonIsVisible(true)
            verifyCollectionMenuIsVisible(true)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, true)
            collapseCollection(collectionName)
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
            closeTab()
        }
        browserScreen {
        }.goToHomescreen {
            expandCollection(collectionName)
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
            expandCollection(collectionName)
            clickShareCollectionButton()
            verifyShareTabsOverlay()
        }
    }

    @Test
    fun deleteCollectionTest() {
        val webPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName)
            snackBarButtonClick("VIEW")
        }
        homeScreen {
            expandCollection(collectionName)
            clickCollectionThreeDotButton()
            selectDeleteCollection()
            confirmDeleteCollection()
            verifyNoCollectionsText()
        }
    }

    @Test
    // Verifies that deleting a Bookmarks folder also removes the item from inside it.
    @Ignore("To be re-implemented in https://github.com/mozilla-mobile/fenix/issues/17799")
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
    fun selectTabsButtonVisibilityTest() {
        homeScreen {
        }.dismissOnboarding()

        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            mDevice.waitForIdle()
        }.openTabDrawer {
        }.toggleToPrivateTabs {
        }.openNewTab {
        }.dismissSearchBar { }

        homeScreen {
        }.openTabDrawer {
        }.toggleToNormalTabs {
            verifySelectTabsButton()
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
            verifyBrowserTabsTrayURL("localhost")
            verifyTabTrayOverflowMenu(true)
            verifyNewTabButton()
        }
    }

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
        }.openHomeScreenShortcut("Private Firefox Preview") {
        }
    }

    @Test
    @Ignore("To be re-implemented in https://github.com/mozilla-mobile/fenix/issues/17799")
    fun mainMenuInstallPWATest() {
        val pwaPage = "https://rpappalax.github.io/testapp/"

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
    @Ignore("To be re-implemented in https://github.com/mozilla-mobile/fenix/issues/17971")
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
}
