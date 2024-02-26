/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import com.google.android.material.bottomsheet.BottomSheetBehavior
import mozilla.components.concept.engine.mediasession.MediaSession
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.closeApp
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestSetup
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

class TabbedBrowsingTest : TestSetup() {
    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903599
    @Test
    fun closeAllTabsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
            verifyShareTabButton()
            verifySelectTabs()
        }.closeAllTabs {
            verifyTabCounter("0")
        }

        // Repeat for Private Tabs
        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openTabDrawer {
            verifyPrivateModeSelected()
            verifyExistingTabList()
        }.openTabsListThreeDotMenu {
            verifyCloseAllTabsButton()
        }.closeAllTabs {
            verifyTabCounter("0")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2349580
    @Test
    fun closingTabsTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            closeTab()
            verifySnackBarText("Tab closed")
            snackBarButtonClick("UNDO")
        }
        browserScreen {
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903604
    @Test
    fun swipeToCloseTabsTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            waitForPageToLoad()
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabRight("Test_Page_1")
            verifySnackBarText("Tab closed")
        }
        homeScreen {
            verifyTabCounter("0")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            waitForPageToLoad()
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            swipeTabLeft("Test_Page_1")
            verifySnackBarText("Tab closed")
        }
        homeScreen {
            verifyTabCounter("0")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903591
    @Test
    fun closingPrivateTabsTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen { }.togglePrivateBrowsingMode(switchPBModeOn = true)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openTabDrawer {
            verifyExistingOpenTabs("Test_Page_1")
            verifyCloseTabsButton("Test_Page_1")
            closeTab()
            verifySnackBarText("Private tab closed")
            snackBarButtonClick("UNDO")
        }
        browserScreen {
            verifyTabCounter("1")
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
        }.openTabDrawer {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903602
    @Test
    fun verifyTabTrayNotShowingStateHalfExpanded() {
        navigationToolbar {
        }.openTabTray {
            verifyNoOpenTabsInNormalBrowsing()
            // With no tabs opened the state should be STATE_COLLAPSED.
            verifyBehaviorState(BottomSheetBehavior.STATE_COLLAPSED)
            // Need to ensure the halfExpandedRatio is very small so that when in STATE_HALF_EXPANDED
            // the tabTray will actually have a very small height (for a very short time) akin to being hidden.
            verifyHalfExpandedRatio()
        }.clickTopBar {
        }.waitForTabTrayBehaviorToIdle {
            // Touching the topBar would normally advance the tabTray to the next state.
            // We don't want that.
            verifyBehaviorState(BottomSheetBehavior.STATE_COLLAPSED)
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
        navigationToolbar {
        }.openTabTray {
            verifyNormalBrowsingButtonIsSelected(true)
            verifyPrivateBrowsingButtonIsSelected(false)
            verifySyncedTabsButtonIsSelected(false)
            verifyNoOpenTabsInNormalBrowsing()
            verifyNormalBrowsingNewTabButton()
            verifyTabTrayOverflowMenu(true)
            verifyEmptyTabsTrayMenuButtons()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903585
    @Test
    fun verifyEmptyPrivateTabsTrayTest() {
        navigationToolbar {
        }.openTabTray {
        }.toggleToPrivateTabs {
            verifyNormalBrowsingButtonIsSelected(false)
            verifyPrivateBrowsingButtonIsSelected(true)
            verifySyncedTabsButtonIsSelected(false)
            verifyNoOpenTabsInPrivateBrowsing()
            verifyPrivateBrowsingNewTabButton()
            verifyTabTrayOverflowMenu(true)
            verifyEmptyTabsTrayMenuButtons()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/903601
    @Test
    fun verifyTabsTrayWithOpenTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(defaultWebPage.url.toString()) {
        }.openTabDrawer {
            verifyNormalBrowsingButtonIsSelected(true)
            verifyPrivateBrowsingButtonIsSelected(false)
            verifySyncedTabsButtonIsSelected(false)
            verifyTabTrayOverflowMenu(true)
            verifyTabsTrayCounter()
            verifyExistingTabList()
            verifyNormalBrowsingNewTabButton()
            verifyOpenedTabThumbnail()
            verifyExistingOpenTabs(defaultWebPage.title)
            verifyCloseTabsButton(defaultWebPage.title)
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
        }.openTabDrawer {
        }.toggleToPrivateTabs {
        }.openNewTab {
        }.submitQuery(website.url.toString()) {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/927314
    @Test
    fun tabsCounterShortcutMenuCloseTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            waitForPageToLoad()
        }.goToHomescreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            waitForPageToLoad()
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }.closeTabFromShortcutsMenu {
            browserScreen {
                verifyTabCounter("1")
                verifyPageContent(firstWebPage.content)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2343663
    @Test
    fun tabsCounterShortcutMenuNewPrivateTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifySearchBarPlaceholder("Search or enter address")
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2343662
    @Test
    fun tabsCounterShortcutMenuNewTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewTabFromShortcutsMenu {
            verifySearchBarPlaceholder("Search or enter address")
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/927315
    @Test
    fun privateTabsCounterShortcutMenuCloseTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        homeScreen {}.togglePrivateBrowsingMode(switchPBModeOn = true)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            waitForPageToLoad()
        }.goToHomescreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            waitForPageToLoad()
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }.closeTabFromShortcutsMenu {
            browserScreen {
                verifyTabCounter("1")
                verifyPageContent(firstWebPage.content)
            }
        }.openTabButtonShortcutsMenu {
        }.closeTabFromShortcutsMenu {
            homeScreen {
                verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = true)
            }
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2344199
    @Test
    fun privateTabsCounterShortcutMenuNewPrivateTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {}.togglePrivateBrowsingMode(switchPBModeOn = true)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifySearchBarPlaceholder("Search or enter address")
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = true)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2344198
    @Test
    fun privateTabsCounterShortcutMenuNewTabTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {}.togglePrivateBrowsingMode(switchPBModeOn = true)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewTabFromShortcutsMenu {
            verifySearchToolbar(isDisplayed = true)
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1046683
    @Test
    fun verifySyncedTabsWhenUserIsNotSignedInTest() {
        navigationToolbar {
        }.openTabTray {
            verifySyncedTabsButtonIsSelected(isSelected = false)
            clickSyncedTabsButton()
        }.toggleToSyncedTabs {
            verifySyncedTabsButtonIsSelected(isSelected = true)
            verifySyncedTabsListWhenUserIsNotSignedIn()
        }.clickSignInToSyncButton {
            verifyTurnOnSyncMenu()
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
        closeApp(activityTestRule)
        restartApp(activityTestRule)
        homeScreen {
            verifyPrivateBrowsingHomeScreenItems()
        }.openTabDrawer {
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
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {}
        closeApp(activityTestRule)
        restartApp(activityTestRule)
        homeScreen {
            verifyPrivateBrowsingHomeScreenItems()
        }.openTabDrawer {
            verifyNoOpenTabsInPrivateBrowsing()
        }
    }
}
