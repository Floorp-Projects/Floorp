/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.swipeDown
import androidx.test.espresso.action.ViewActions.swipeUp
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.checkedItemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.nimbus.FxNimbus

/**
 * Implementation of Robot Pattern for the three dot (main) menu.
 */
@Suppress("ForbiddenComment")
class ThreeDotMenuMainRobot {
    fun verifyShareAllTabsButton() {
        Log.i(TAG, "verifyShareAllTabsButton: Trying to verify that the \"Share all tabs\" menu button is displayed")
        shareAllTabsButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareAllTabsButton: Verified that the \"Share all tabs\" menu button is displayed")
    }
    fun verifySettingsButton() = assertUIObjectExists(settingsButton())
    fun verifyHistoryButton() = assertUIObjectExists(historyButton())
    fun verifyThreeDotMenuExists() {
        Log.i(TAG, "verifyThreeDotMenuExists: Trying to verify that the three dot menu is displayed")
        threeDotMenuRecyclerView().check(matches(isDisplayed()))
        Log.i(TAG, "verifyThreeDotMenuExists: Verified that the three dot menu is displayed")
    }
    fun verifyAddBookmarkButton() = assertUIObjectExists(addBookmarkButton())
    fun verifyEditBookmarkButton() {
        Log.i(TAG, "verifyEditBookmarkButton: Trying to verify that the \"Edit\" button is visible")
        editBookmarkButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyEditBookmarkButton: Verified that the \"Edit\" button is visible")
    }
    fun verifyCloseAllTabsButton() {
        Log.i(TAG, "verifyCloseAllTabsButton: Trying to verify that the \"Close all tabs\" button is visible")
        closeAllTabsButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseAllTabsButton: Verified that the \"Close all tabs\" button is visible")
    }
    fun verifyReaderViewAppearance(visible: Boolean) {
        var maxSwipes = 3
        if (visible) {
            while (!readerViewAppearanceToggle().exists() && maxSwipes != 0) {
                Log.i(TAG, "verifyReaderViewAppearance: The \"Customize reader view\" button does not exist")
                Log.i(TAG, "verifyReaderViewAppearance: Trying to perform swipe up action on the three dot menu")
                threeDotMenuRecyclerView().perform(swipeUp())
                Log.i(TAG, "verifyReaderViewAppearance: Performed swipe up action on the three dot menu")
                maxSwipes--
            }
            assertUIObjectExists(readerViewAppearanceToggle())
        } else {
            while (!readerViewAppearanceToggle().exists() && maxSwipes != 0) {
                Log.i(TAG, "verifyReaderViewAppearance: The \"Customize reader view\" button does not exist")
                Log.i(TAG, "verifyReaderViewAppearance: Trying to perform swipe up action on the three dot menu")
                threeDotMenuRecyclerView().perform(swipeUp())
                Log.i(TAG, "verifyReaderViewAppearance: Performed swipe up action on the three dot menu")
                maxSwipes--
            }
            assertUIObjectExists(readerViewAppearanceToggle(), exists = false)
        }
    }

    fun verifyQuitButtonExists() {
        // Need to double swipe the menu, to make this button visible.
        // In case it reaches the end, the second swipe is no-op.
        expandMenu()
        expandMenu()
        assertUIObjectExists(itemWithText("Quit"))
    }

    fun expandMenu() {
        Log.i(TAG, "expandMenu: Trying to perform swipe up action on the three dot menu")
        onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeUp())
        Log.i(TAG, "expandMenu: Performed swipe up action on the three dot menu")
    }

    fun verifyShareTabButton() {
        Log.i(TAG, "verifyShareTabButton: Trying to verify that the \"Share all tabs\" button is visible")
        shareTabButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyShareTabButton: Verified that the \"Share all tabs\" button is visible")
    }
    fun verifySelectTabs() {
        Log.i(TAG, "verifySelectTabs: Trying to verify that the \"Select tabs\" button is visible")
        selectTabsButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifySelectTabs: Verified that the \"Select tabs\" button is visible")
    }

    fun verifyFindInPageButton() = assertUIObjectExists(findInPageButton())
    fun verifyAddToShortcutsButton(shouldExist: Boolean) =
        assertUIObjectExists(addToShortcutsButton(), exists = shouldExist)
    fun verifyRemoveFromShortcutsButton() {
        Log.i(TAG, "verifyRemoveFromShortcutsButton: Trying to perform scroll action to the \"Settings\" button")
        onView(withId(R.id.mozac_browser_menu_recyclerView))
            .perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText(R.string.browser_menu_settings)),
                ),
            ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyRemoveFromShortcutsButton: Performed scroll action to the \"Settings\" button")
    }

    fun verifyShareTabsOverlay() {
        Log.i(TAG, "verifyShareTabsOverlay: Trying to verify that the share overlay site list is displayed")
        onView(withId(R.id.shared_site_list)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareTabsOverlay: Verified that the share overlay site list is displayed")
        Log.i(TAG, "verifyShareTabsOverlay: Trying to verify that the shared tab title is displayed")
        onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareTabsOverlay: Verified that the shared tab title is displayed")
        Log.i(TAG, "verifyShareTabsOverlay: Trying to verify that the shared tab favicon is displayed")
        onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareTabsOverlay: Verified that the shared tab favicon is displayed")
        Log.i(TAG, "verifyShareTabsOverlay: Trying to verify that the shared tab url is displayed")
        onView(withId(R.id.share_tab_url)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareTabsOverlay: Verified that the shared tab url is displayed")
    }

    fun verifyDesktopSiteModeEnabled(isRequestDesktopSiteEnabled: Boolean) {
        expandMenu()
        assertUIObjectExists(desktopSiteToggle(isRequestDesktopSiteEnabled))
    }

    fun verifyPageThreeDotMainMenuItems(isRequestDesktopSiteEnabled: Boolean) {
        expandMenu()
        assertUIObjectExists(
            normalBrowsingNewTabButton(),
            bookmarksButton(),
            historyButton(),
            downloadsButton(),
            addOnsButton(),
            syncAndSaveDataButton(),
            findInPageButton(),
            desktopSiteButton(),
            reportSiteIssueButton(),
            addToHomeScreenButton(),
            addToShortcutsButton(),
            saveToCollectionButton(),
            addBookmarkButton(),
            desktopSiteToggle(isRequestDesktopSiteEnabled),
            translateButton(),
        )
        // Swipe to second part of menu
        expandMenu()
        assertUIObjectExists(
            settingsButton(),
        )
        if (FxNimbus.features.print.value().browserPrintEnabled) {
            assertUIObjectExists(printContentButton())
        }
        assertUIObjectExists(
            backButton(),
            forwardButton(),
            shareButton(),
            refreshButton(),
        )
    }

    fun verifyHomeThreeDotMainMenuItems(isRequestDesktopSiteEnabled: Boolean) {
        assertUIObjectExists(
            bookmarksButton(),
            historyButton(),
            downloadsButton(),
            addOnsButton(),
            // Disabled step due to https://github.com/mozilla-mobile/fenix/issues/26788
            // syncAndSaveDataButton,
            desktopSiteButton(),
            whatsNewButton(),
            helpButton(),
            customizeHomeButton(),
            settingsButton(),
            desktopSiteToggle(isRequestDesktopSiteEnabled),
        )
    }

    fun openAddonsSubList() {
        // when there are add-ons installed, there is an overflow Add-ons sub-menu
        // in that case we use this method instead or before openAddonsManagerMenu()
        clickAddonsManagerButton()
    }

    fun verifyAddonAvailableInMainMenu(addonName: String) {
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "verifyAddonAvailableInMainMenu: Started try #$i")
            try {
                assertUIObjectExists(itemContainingText(addonName))
                break
            } catch (e: AssertionError) {
                Log.i(TAG, "verifyAddonAvailableInMainMenu: AssertionError caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    mDevice.pressBack()
                    browserScreen {
                    }.openThreeDotMenu {
                        openAddonsSubList()
                    }
                }
            }
        }
    }

    fun verifyTrackersBlockedByUblock() {
        assertUIObjectExists(itemWithResId("$packageName:id/badge_text"))
        Log.i(TAG, "verifyTrackersBlockedByUblock: Trying to verify that the count of trackers blocked is greater than 0")
        assertTrue("$TAG: The count of trackers blocked is not greater than 0", itemWithResId("$packageName:id/badge_text").text.toInt() > 0)
        Log.i(TAG, "verifyTrackersBlockedByUblock: Verified that the count of trackers blocked is greater than 0")
    }

    fun clickQuit() {
        expandMenu()
        Log.i(TAG, "clickQuit: Trying to click the \"Quit\" button")
        onView(withText("Quit")).click()
        Log.i(TAG, "clickQuit: Clicked the \"Quit\" button")
    }

    class Transition {
        fun openSettings(
            localizedText: String = getStringResource(R.string.browser_menu_settings),
            interact: SettingsRobot.() -> Unit,
        ): SettingsRobot.Transition {
            // We require one swipe to display the full size 3-dot menu. On smaller devices
            // such as the Pixel 2, we require two swipes to display the "Settings" menu item
            // at the bottom. On larger devices, the second swipe is a no-op.
            Log.i(TAG, "openSettings: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openSettings: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openSettings: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openSettings: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openSettings: Trying to click the $localizedText button")
            settingsButton(localizedText).click()
            Log.i(TAG, "openSettings: Clicked the $localizedText button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openDownloadsManager(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
            Log.i(TAG, "openDownloadsManager: Trying to perform swipe down action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeDown())
            Log.i(TAG, "openDownloadsManager: Performed swipe down action on the three dot menu")
            Log.i(TAG, "openDownloadsManager: Trying to click the \"DOWNLOADS\" button")
            downloadsButton().click()
            Log.i(TAG, "openDownloadsManager: Clicked the \"DOWNLOADS\" button")

            DownloadRobot().interact()
            return DownloadRobot.Transition()
        }

        fun openSyncSignIn(interact: SyncSignInRobot.() -> Unit): SyncSignInRobot.Transition {
            Log.i(TAG, "openSyncSignIn: Trying to perform swipe down action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeDown())
            Log.i(TAG, "openSyncSignIn: Performed swipe down action on the three dot menu")
            mDevice.waitNotNull(Until.findObject(By.text("Sync and save data")), waitingTime)
            Log.i(TAG, "openSyncSignIn: Trying to click the \"Sync and save data\" button")
            syncAndSaveDataButton().click()
            Log.i(TAG, "openSyncSignIn: Clicked the \"Sync and save data\" button")

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun openBookmarks(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "openBookmarks: Trying to perform swipe down action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeDown())
            Log.i(TAG, "openBookmarks: Performed swipe down action on the three dot menu")
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)
            Log.i(TAG, "openBookmarks: Trying to click the \"Bookmarks\" button")
            bookmarksButton().click()
            Log.i(TAG, "openBookmarks: Clicked the \"Bookmarks\" button")
            assertUIObjectExists(itemWithResId("$packageName:id/bookmark_list"))

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickNewTabButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "clickNewTabButton: Trying to click the \"New tab\" button")
            normalBrowsingNewTabButton().click()
            Log.i(TAG, "clickNewTabButton: Clicked the \"New tab\" button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun openHistory(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            Log.i(TAG, "openHistory: Trying to perform swipe down action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeDown())
            Log.i(TAG, "openHistory: Performed swipe down action on the three dot menu")
            mDevice.waitNotNull(Until.findObject(By.text("History")), waitingTime)
            Log.i(TAG, "openHistory: Trying to click the \"History\" button")
            historyButton().click()
            Log.i(TAG, "openHistory: Clicked the \"History\" button")

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun bookmarkPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)
            Log.i(TAG, "bookmarkPage: Trying to click the \"Add\" button")
            addBookmarkButton().click()
            Log.i(TAG, "bookmarkPage: Clicked the \"Add\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun editBookmarkPage(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)
            Log.i(TAG, "editBookmarkPage: Trying to click the \"Edit\" button")
            editBookmarkButton().click()
            Log.i(TAG, "editBookmarkPage: Clicked the \"Edit\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun openHelp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Help")), waitingTime)
            Log.i(TAG, "openHelp: Trying to click the \"Help\" button")
            helpButton().click()
            Log.i(TAG, "openHelp: Clicked the \"Help\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openCustomizeHome(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            Log.i(TAG, "openCustomizeHome: Waiting for $waitingTime ms until finding the \"Customize homepage\" button")
            mDevice.wait(
                Until
                    .findObject(
                        By.textContains("$packageName:id/browser_menu_customize_home_1"),
                    ),
                waitingTime,
            )
            Log.i(TAG, "openCustomizeHome: Waited for $waitingTime ms until the \"Customize homepage\" button was found")
            Log.i(TAG, "openCustomizeHome: Trying to click the \"Customize homepage\" button")
            customizeHomeButton().click()
            Log.i(TAG, "openCustomizeHome: Clicked the \"Customize homepage\" button")
            Log.i(TAG, "openCustomizeHome: Waiting for $waitingTime ms for \"Customize homepage\" settings menu to exist")
            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/recycler_view"),
            ).waitForExists(waitingTime)
            Log.i(TAG, "openCustomizeHome: Waited for $waitingTime ms for \"Customize homepage\" settings menu to exist")

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun goForward(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "goForward: Trying to click the \"Forward\" button")
            forwardButton().click()
            Log.i(TAG, "goForward: Clicked the \"Forward\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goToPreviousPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "goToPreviousPage: Trying to click the \"Back\" button")
            backButton().click()
            Log.i(TAG, "goToPreviousPage: Clicked the \"Back\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            Log.i(TAG, "clickShareButton: Trying to click the \"Share\" button")
            shareButton().click()
            Log.i(TAG, "clickShareButton: Clicked the \"Share\" button")
            mDevice.waitNotNull(Until.findObject(By.text("ALL ACTIONS")), waitingTime)

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun closeBrowserMenuToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "closeBrowserMenuToBrowser: Trying to click device back button")
            // Close three dot
            mDevice.pressBack()
            Log.i(TAG, "closeBrowserMenuToBrowser: Clicked the device back button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun refreshPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            refreshButton().also {
                Log.i(TAG, "refreshPage: Waiting for $waitingTime ms for the \"Refresh\" button to exist")
                it.waitForExists(waitingTime)
                Log.i(TAG, "refreshPage: Waited for $waitingTime ms for the \"Refresh\" button to exist")
                Log.i(TAG, "refreshPage: Trying to click the \"Refresh\" button")
                it.click()
                Log.i(TAG, "refreshPage: Clicked the \"Refresh\" button")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun forceRefreshPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "forceRefreshPage: Trying to long click the \"Refresh\" button")
            mDevice.findObject(By.desc(getStringResource(R.string.browser_menu_refresh)))
                .click(LONG_CLICK_DURATION)
            Log.i(TAG, "forceRefreshPage: Long clicked the \"Refresh\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeAllTabs(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            Log.i(TAG, "closeAllTabs: Trying to click the \"Close all tabs\" button")
            closeAllTabsButton().click()
            Log.i(TAG, "closeAllTabs: Clicked the \"Close all tabs\" button")

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openReportSiteIssue(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openReportSiteIssue: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openReportSiteIssue: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openReportSiteIssue: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openReportSiteIssue: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openReportSiteIssue: Trying to click the \"Report Site Issue\" button")
            reportSiteIssueButton().click()
            Log.i(TAG, "openReportSiteIssue: Clicked the \"Report Site Issue\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openFindInPage(interact: FindInPageRobot.() -> Unit): FindInPageRobot.Transition {
            Log.i(TAG, "openFindInPage: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openFindInPage: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openFindInPage: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openFindInPage: Performed swipe up action on the three dot menu")
            mDevice.waitNotNull(Until.findObject(By.text("Find in page")), waitingTime)
            Log.i(TAG, "openFindInPage: Trying to click the \"Find in page\" button")
            findInPageButton().click()
            Log.i(TAG, "openFindInPage: Clicked the \"Find in page\" button")

            FindInPageRobot().interact()
            return FindInPageRobot.Transition()
        }

        fun openWhatsNew(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("What’s new")), waitingTime)
            Log.i(TAG, "openWhatsNew: Trying to click the \"What’s new\" button")
            whatsNewButton().click()
            Log.i(TAG, "openWhatsNew: Clicked the \"What’s new\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openReaderViewAppearance(interact: ReaderViewRobot.() -> Unit): ReaderViewRobot.Transition {
            Log.i(TAG, "openReaderViewAppearance: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openReaderViewAppearance: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openReaderViewAppearance: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openReaderViewAppearance: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openReaderViewAppearance: Trying to click the \"Customize reader view\" button")
            readerViewAppearanceToggle().click()
            Log.i(TAG, "openReaderViewAppearance: Clicked the \"Customize reader view\" button")

            ReaderViewRobot().interact()
            return ReaderViewRobot.Transition()
        }

        fun addToFirefoxHome(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            for (i in 1..RETRY_COUNT) {
                Log.i(TAG, "addToFirefoxHome: Started try #$i")
                try {
                    addToShortcutsButton().also {
                        Log.i(TAG, "addToFirefoxHome: Waiting for $waitingTime ms for the \"Add to shortcuts\" button to exist")
                        it.waitForExists(waitingTime)
                        Log.i(TAG, "addToFirefoxHome: Waited for $waitingTime ms for the \"Add to shortcuts\" button to exist")
                        Log.i(TAG, "addToFirefoxHome: Trying to click the \"Add to shortcuts\" button")
                        it.click()
                        Log.i(TAG, "addToFirefoxHome: Clicked the \"Add to shortcuts\" button")
                    }

                    break
                } catch (e: UiObjectNotFoundException) {
                    Log.i(TAG, "addToFirefoxHome: UiObjectNotFoundException caught, executing fallback methods")
                    if (i == RETRY_COUNT) {
                        throw e
                    } else {
                        Log.i(TAG, "addToFirefoxHome: Trying to click the device back button")
                        mDevice.pressBack()
                        Log.i(TAG, "addToFirefoxHome: Clicked the device back button")
                        navigationToolbar {
                        }.openThreeDotMenu {
                            expandMenu()
                        }
                    }
                }
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickRemoveFromShortcuts(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickRemoveFromShortcuts: Trying to click the \"Remove from shortcuts\" button")
            removeFromShortcutsButton().click()
            Log.i(TAG, "clickRemoveFromShortcuts: Clicked the \"Remove from shortcuts\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openAddToHomeScreen(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            Log.i(TAG, "openAddToHomeScreen: Trying to click the \"Add to Home screen\" button and wait for $waitingTime ms for a new window")
            addToHomeScreenButton().clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "openAddToHomeScreen: Clicked the \"Add to Home screen\" button and waited for $waitingTime ms for a new window")

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun clickInstall(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            Log.i(TAG, "clickInstall: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "clickInstall: Performed swipe up action on the three dot menu")
            Log.i(TAG, "clickInstall: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "clickInstall: Performed swipe up action on the three dot menu")
            Log.i(TAG, "clickInstall: Trying to click the \"Install\" button")
            installPWAButton().click()
            Log.i(TAG, "clickInstall: Clicked the \"Install\" button")

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun openSaveToCollection(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            // Ensure the menu is expanded and fully scrolled to the bottom.
            Log.i(TAG, "openSaveToCollection: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openSaveToCollection: Performed swipe up action on the three dot menu")
            Log.i(TAG, "openSaveToCollection: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "openSaveToCollection: Performed swipe up action on the three dot menu")

            mDevice.waitNotNull(Until.findObject(By.text("Save to collection")), waitingTime)
            Log.i(TAG, "openSaveToCollection: Trying to click the \"Save to collection\" button")
            saveToCollectionButton().click()
            Log.i(TAG, "openSaveToCollection: Clicked the \"Save to collection\" button")
            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }

        fun openAddonsManagerMenu(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            clickAddonsManagerButton()
            Log.i(TAG, "openAddonsManagerMenu: Waiting for $waitingTimeLong ms for the addons list to exist")
            mDevice.findObject(UiSelector().resourceId("$packageName:id/add_ons_list"))
                .waitForExists(waitingTimeLong)
            Log.i(TAG, "openAddonsManagerMenu: Waited for $waitingTimeLong ms for the addons list to exist")

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }

        fun clickOpenInApp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickOpenInApp: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "clickOpenInApp: Performed swipe up action on the three dot menu")
            Log.i(TAG, "clickOpenInApp: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "clickOpenInApp: Performed swipe up action on the three dot menu")
            Log.i(TAG, "clickOpenInApp: Trying to click the \"Open in app\" button")
            openInAppButton().click()
            Log.i(TAG, "clickOpenInApp: Clicked the \"Open in app\" button")
            Log.i(TAG, "clickOpenInApp: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "clickOpenInApp: Waited for device to be idle")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun switchDesktopSiteMode(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "switchDesktopSiteMode: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "switchDesktopSiteMode: Performed swipe up action on the three dot menu")
            Log.i(TAG, "switchDesktopSiteMode: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "switchDesktopSiteMode: Performed swipe up action on the three dot menu")
            Log.i(TAG, "switchDesktopSiteMode: Trying to click the \"Desktop site\" button")
            desktopSiteButton().click()
            Log.i(TAG, "switchDesktopSiteMode: Clicked the \"Desktop site\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareAllTabsButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            Log.i(TAG, "clickShareAllTabsButton: Trying to click the \"Share all tabs\" button")
            shareAllTabsButton().click()
            Log.i(TAG, "clickShareAllTabsButton: Clicked the \"Share all tabs\" button")

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun clickPrintButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickPrintButton: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "clickPrintButton: Performed swipe up action on the three dot menu")
            Log.i(TAG, "clickPrintButton: Trying to perform swipe up action on the three dot menu")
            threeDotMenuRecyclerView().perform(swipeUp())
            Log.i(TAG, "clickPrintButton: Performed swipe up action on the three dot menu")
            Log.i(TAG, "clickPrintButton: Waiting for $waitingTime ms for the \"Print\" button to exist")
            printButton().waitForExists(waitingTime)
            Log.i(TAG, "clickPrintButton: Waited for $waitingTime ms for the \"Print\" button to exist")
            Log.i(TAG, "clickPrintButton: Trying to click the \"Print\" button")
            printButton().click()
            Log.i(TAG, "clickPrintButton: Clicked the \"Print\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}
private fun threeDotMenuRecyclerView() =
    onView(withId(R.id.mozac_browser_menu_recyclerView))

private fun editBookmarkButton() = onView(withText("Edit"))

private fun stopLoadingButton() = onView(ViewMatchers.withContentDescription("Stop"))

private fun closeAllTabsButton() = onView(allOf(withText("Close all tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun shareTabButton() = onView(allOf(withText("Share all tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun selectTabsButton() = onView(allOf(withText("Select tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun readerViewAppearanceToggle() =
    mDevice.findObject(UiSelector().text("Customize reader view"))

private fun removeFromShortcutsButton() =
    onView(allOf(withText(R.string.browser_menu_remove_from_shortcuts)))

private fun installPWAButton() =
    itemContainingText(getStringResource(R.string.browser_menu_add_to_homescreen))

private fun openInAppButton() =
    onView(
        allOf(
            withText("Open in app"),
            withEffectiveVisibility(Visibility.VISIBLE),
        ),
    )

private fun clickAddonsManagerButton() {
    Log.i(TAG, "clickAddonsManagerButton: Trying to perform swipe down action on the three dot menu")
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeDown())
    Log.i(TAG, "clickAddonsManagerButton: Performed swipe down action on the three dot menu")
    Log.i(TAG, "clickAddonsManagerButton: Trying to click the \"Add-ons\" button")
    addOnsButton().click()
    Log.i(TAG, "clickAddonsManagerButton: Clicked the \"Add-ons\" button")
}

private fun shareAllTabsButton() =
    onView(allOf(withText("Share all tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun bookmarksButton() =
    itemContainingText(getStringResource(R.string.library_bookmarks))
private fun historyButton() =
    itemContainingText(getStringResource(R.string.library_history))
private fun downloadsButton() =
    itemContainingText(getStringResource(R.string.library_downloads))
private fun addOnsButton() =
    itemContainingText(getStringResource(R.string.browser_menu_extensions))
private fun desktopSiteButton() =
    itemContainingText(getStringResource(R.string.browser_menu_desktop_site))
private fun desktopSiteToggle(state: Boolean) =
    checkedItemWithResIdAndText(
        "$packageName:id/switch_widget",
        getStringResource(R.string.browser_menu_desktop_site),
        state,
    )
private fun whatsNewButton() =
    itemContainingText(getStringResource(R.string.browser_menu_whats_new))
private fun helpButton() =
    itemContainingText(getStringResource(R.string.browser_menu_help))
private fun customizeHomeButton() =
    itemContainingText(getStringResource(R.string.browser_menu_customize_home_1))
private fun settingsButton(localizedText: String = getStringResource(R.string.browser_menu_settings)) =
    itemContainingText(localizedText)
private fun syncAndSaveDataButton() =
    itemContainingText(getStringResource(R.string.sync_menu_sync_and_save_data))
private fun normalBrowsingNewTabButton() =
    itemContainingText(getStringResource(R.string.library_new_tab))
private fun addBookmarkButton() =
    itemWithResIdAndText(
        "$packageName:id/checkbox",
        getStringResource(R.string.browser_menu_add),
    )
private fun findInPageButton() = itemContainingText(getStringResource(R.string.browser_menu_find_in_page))
private fun translateButton() = itemContainingText(getStringResource(R.string.browser_menu_translations))
private fun reportSiteIssueButton() = itemContainingText("Report Site Issue")
private fun addToHomeScreenButton() = itemContainingText(getStringResource(R.string.browser_menu_add_to_homescreen))
private fun addToShortcutsButton() = itemContainingText(getStringResource(R.string.browser_menu_add_to_shortcuts))
private fun saveToCollectionButton() = itemContainingText(getStringResource(R.string.browser_menu_save_to_collection_2))
private fun printContentButton() = itemContainingText(getStringResource(R.string.menu_print))
private fun backButton() = itemWithDescription(getStringResource(R.string.browser_menu_back))
private fun forwardButton() = itemWithDescription(getStringResource(R.string.browser_menu_forward))
private fun shareButton() = itemWithDescription(getStringResource(R.string.share_button_content_description))
private fun refreshButton() = itemWithDescription(getStringResource(R.string.browser_menu_refresh))
private fun printButton() = itemWithText("Print")
