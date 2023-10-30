/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import com.google.android.material.bottomsheet.BottomSheetBehavior
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.AppAndSystemHelper.verifyKeyboardVisibility
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.closeApp
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade

/**
 *  Tests for verifying basic functionality of tabbed browsing
 *
 *  Including:
 *  - Opening a tab
 *  - Opening a private tab
 *  - Verifying tab list
 *  - Closing all tabs
 *  - Close tab
 *  - Swipe to close tab (temporarily disabled)
 *  - Undo close tab
 *  - Close private tabs persistent notification
 *  - Empty tab tray state
 *  - Tab tray details
 *  - Shortcut context menu navigation
 */

class ComposeTabbedBrowsingTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer
    private lateinit var browserStore: BrowserStore

    @get:Rule(order = 0)
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(
                skipOnboarding = true,
                tabsTrayRewriteEnabled = true,
            ),
        ) { it.activity }

    @Rule(order = 1)
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        // Initializing this as part of class construction, below the rule would throw a NPE
        // So we are initializing this here instead of in all related tests.
        browserStore = composeTestRule.activity.components.core.store

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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903599
    @Test
    fun closeAllTabsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyNormalTabsList()
        }.openThreeDotMenu {
            verifyCloseAllTabsButton()
            verifyShareAllTabsButton()
            verifySelectTabsButton()
        }.closeAllTabs {
            verifyTabCounter("0")
        }

        // Repeat for Private Tabs
        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyPrivateTabsList()
        }.openThreeDotMenu {
            verifyCloseAllTabsButton()
        }.closeAllTabs {
            verifyTabCounter("0")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903604
    @Test
    fun closingTabsMethodsTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            closeTab()
            verifySnackBarText("Tab closed")
            clickSnackbarButton("UNDO")
        }
        browserScreen {
            verifyTabCounter("1")
        }.openComposeTabDrawer(composeTestRule) {
            closeTab()
        }
        homeScreen {
            verifyTabCounter("0")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabRight("Test_Page_1")
        }
        homeScreen {
            verifyTabCounter("0")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabLeft("Test_Page_1")
        }
        homeScreen {
            verifyTabCounter("0")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903591
    @Test
    fun closingPrivateTabsMethodsTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen { }.togglePrivateBrowsingMode()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            closeTab()
            verifySnackBarText("Private tab closed")
            clickSnackbarButton("UNDO")
        }
        browserScreen {
            verifyTabCounter("1")
        }.openComposeTabDrawer(composeTestRule) {
            closeTab()
        }
        homeScreen {
            verifyTabCounter("0")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabRight("Test_Page_1")
            verifySnackBarText("Private tab closed")
        }
        homeScreen {
            verifyTabCounter("0")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabLeft("Test_Page_1")
            verifySnackBarText("Private tab closed")
        }
        homeScreen {
            verifyTabCounter("0")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903606
    @SmokeTest
    @Test
    fun tabMediaControlButtonTest() {
        val audioTestPage = TestAssetHelper.getAudioPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(audioTestPage.url) {
            mDevice.waitForIdle()
            clickPageObject(MatcherHelper.itemWithText("Play"))
            assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
        }.openComposeTabDrawer(composeTestRule) {
            verifyTabMediaControlButtonState("Pause")
            clickTabMediaControlButton("Pause")
            verifyTabMediaControlButtonState("Play")
        }.openTab(audioTestPage.title) {
            assertPlaybackState(browserStore, MediaSession.PlaybackState.PAUSED)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903592
    @SmokeTest
    @Test
    fun verifyCloseAllPrivateTabsNotificationTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.openNotification()
        }

        notificationShade {
            verifyPrivateTabsNotification()
        }.clickClosePrivateTabsNotification {
            verifyHomeScreen()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903602
    @Test
    fun verifyTabTrayNotShowingStateHalfExpanded() {
        homeScreen {
        }.openComposeTabDrawer(composeTestRule) {
            verifyNoOpenTabsInNormalBrowsing()
            // With no tabs opened the state should be STATE_COLLAPSED.
            verifyTabsTrayBehaviorState(BottomSheetBehavior.STATE_COLLAPSED)
            // Need to ensure the halfExpandedRatio is very small so that when in STATE_HALF_EXPANDED
            // the tabTray will actually have a very small height (for a very short time) akin to being hidden.
            verifyMinusculeHalfExpandedRatio()
        }.clickTopBar {
        }.waitForTabTrayBehaviorToIdle {
            // Touching the topBar would normally advance the tabTray to the next state.
            // We don't want that.
            verifyTabsTrayBehaviorState(BottomSheetBehavior.STATE_COLLAPSED)
        }.advanceToHalfExpandedState {
        }.waitForTabTrayBehaviorToIdle {
            // TabTray should not be displayed in STATE_HALF_EXPANDED.
            // When advancing to this state it should immediately be hidden.
            verifyTabTrayIsClosed()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903600
    @Test
    fun verifyEmptyTabTray() {
        homeScreen {
        }.openComposeTabDrawer(composeTestRule) {
            verifyNormalBrowsingButtonIsSelected()
            verifyPrivateBrowsingButtonIsSelected(false)
            verifySyncedTabsButtonIsSelected(false)
            verifyNoOpenTabsInNormalBrowsing()
            verifyFab()
            verifyThreeDotButton()
        }.openThreeDotMenu {
            verifyTabSettingsButton()
            verifyRecentlyClosedTabsButton()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903585
    @Test
    fun verifyEmptyPrivateTabsTrayTest() {
        homeScreen {
        }.openComposeTabDrawer(composeTestRule) {
        }.toggleToPrivateTabs {
            verifyNormalBrowsingButtonIsSelected(false)
            verifyPrivateBrowsingButtonIsSelected(true)
            verifySyncedTabsButtonIsSelected(false)
            verifyNoOpenTabsInPrivateBrowsing()
            verifyFab()
            verifyThreeDotButton()
        }.openThreeDotMenu {
            verifyTabSettingsButton()
            verifyRecentlyClosedTabsButton()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903601
    @Test
    fun verifyTabsTrayWithOpenTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyNormalBrowsingButtonIsSelected()
            verifyPrivateBrowsingButtonIsSelected(isSelected = false)
            verifySyncedTabsButtonIsSelected(isSelected = false)
            verifyThreeDotButton()
            verifyNormalTabCounter()
            verifyNormalTabsList()
            verifyFab()
            verifyTabThumbnail()
            verifyExistingOpenTabs(defaultWebPage.title)
            verifyTabCloseButton()
        }.openTab(defaultWebPage.title) {
            verifyUrl(defaultWebPage.url.toString())
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903587
    @SmokeTest
    @Test
    fun verifyPrivateTabsTrayWithOpenTabTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openComposeTabDrawer(composeTestRule) {
        }.toggleToPrivateTabs {
        }.openNewTab {
        }.submitQuery(website.url.toString()) {
        }.openComposeTabDrawer(composeTestRule) {
            verifyNormalBrowsingButtonIsSelected(false)
            verifyPrivateBrowsingButtonIsSelected(true)
            verifySyncedTabsButtonIsSelected(false)
            verifyThreeDotButton()
            verifyNormalTabCounter()
            verifyPrivateTabsList()
            verifyExistingOpenTabs(website.title)
            verifyTabCloseButton()
            verifyTabThumbnail()
            verifyFab()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/927315
    @Test
    fun tabsCounterShortcutMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }.closeTabFromShortcutsMenu {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifyKeyboardVisibility()
            verifySearchBarPlaceholder("Search or enter address")
            // dismiss search dialog
        }.dismissSearchBar {
            verifyPrivateBrowsingHomeScreenItems()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewTabFromShortcutsMenu {
            verifyKeyboardVisibility()
            verifySearchBarPlaceholder("Search or enter address")
            // dismiss search dialog
        }.dismissSearchBar {
            verifyHomeWordmark()
            verifyNavigationToolbar()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/927314
    @Test
    fun privateTabsCounterShortcutMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {}.togglePrivateBrowsingMode()
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }.closeTabFromShortcutsMenu {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifyKeyboardVisibility()
            verifySearchBarPlaceholder("Search or enter address")
            // dismiss search dialog
        }.dismissSearchBar {
            verifyCommonMythsLink()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewTabFromShortcutsMenu {
            verifyKeyboardVisibility()
            verifySearchBarPlaceholder("Search or enter address")
            // dismiss search dialog
        }.dismissSearchBar {
            // Verify normal browsing homescreen
            verifyExistingTopSitesList()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1046683
    @Test
    fun verifySyncedTabsWhenUserIsNotSignedInTest() {
        navigationToolbar {
        }.openComposeTabDrawer(composeTestRule) {
            verifySyncedTabsButtonIsSelected(isSelected = false)
        }.toggleToSyncedTabs {
            verifySyncedTabsButtonIsSelected(isSelected = true)
            verifySyncedTabsListWhenUserIsNotSignedIn()
        }.clickSignInToSyncButton {
            verifyTurnOnSyncMenu()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903598
    @SmokeTest
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
        }.openComposeTabDrawer(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondWebsite.url.toString()) {
            verifyPageContent(secondWebsite.content)
        }.openComposeTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            verifyExistingOpenTabs("Test_Page_2")
        }.openThreeDotMenu {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/526244
    @Test
    fun privateModeStaysAsDefaultAfterRestartTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.goToHomescreen {
        }.togglePrivateBrowsingMode()

        closeApp(composeTestRule.activityRule)
        restartApp(composeTestRule.activityRule)

        homeScreen {
            verifyPrivateBrowsingHomeScreenItems()
        }.openComposeTabDrawer(composeTestRule) {
        }.toggleToNormalTabs {
            verifyExistingOpenTabs(defaultWebPage.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2228470
    @SmokeTest
    @Test
    fun privateTabsDoNotPersistAfterClosingAppTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openComposeTabDrawer(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
        }
        closeApp(composeTestRule.activityRule)
        restartApp(composeTestRule.activityRule)
        homeScreen {
            verifyPrivateBrowsingHomeScreenItems()
        }.openComposeTabDrawer(composeTestRule) {
            verifyNoOpenTabsInPrivateBrowsing()
        }
    }
}
