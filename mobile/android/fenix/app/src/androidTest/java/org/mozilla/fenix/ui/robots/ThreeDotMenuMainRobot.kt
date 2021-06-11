/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

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
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withResourceName
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.share.ShareFragment

/**
 * Implementation of Robot Pattern for the three dot (main) menu.
 */
@Suppress("ForbiddenComment")
class ThreeDotMenuMainRobot {
    fun verifyTabSettingsButton() = assertTabSettingsButton()
    fun verifyRecentlyClosedTabsButton() = assertRecentlyClosedTabsButton()
    fun verifyShareAllTabsButton() = assertShareAllTabsButton()
    fun clickShareAllTabsButton() = shareAllTabsButton().click()
    fun verifySettingsButton() = assertSettingsButton()
    fun verifyAddOnsButton() = assertAddOnsButton()
    fun verifyHistoryButton() = assertHistoryButton()
    fun verifyBookmarksButton() = assertBookmarksButton()
    fun verifySyncSignInButton() = assertSignInToSyncButton()
    fun verifyHelpButton() = assertHelpButton()
    fun verifyThreeDotMenuExists() = threeDotMenuRecyclerViewExists()
    fun verifyForwardButton() = assertForwardButton()
    fun verifyAddBookmarkButton() = assertAddBookmarkButton()
    fun verifyEditBookmarkButton() = assertEditBookmarkButton()
    fun verifyRefreshButton() = assertRefreshButton()
    fun verifyCloseAllTabsButton() = assertCloseAllTabsButton()
    fun verifyShareButton() = assertShareButton()
    fun verifyReaderViewAppearance(visible: Boolean) = assertReaderViewAppearanceButton(visible)

    fun expandMenu() {
        onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeUp())
    }

    fun clickShareButton() {
        var maxSwipes = 3
        while (!shareButton().exists() && maxSwipes != 0) {
            threeDotMenuRecyclerView().perform(swipeUp())
            maxSwipes--
        }
        shareButton().click()
        mDevice.waitNotNull(Until.findObject(By.text("ALL ACTIONS")), waitingTime)
    }

    fun verifyShareTabButton() = assertShareTabButton()
    fun verifySaveCollection() = assertSaveCollectionButton()
    fun verifySelectTabs() = assertSelectTabsButton()

    fun verifyFindInPageButton() = assertFindInPageButton()
    fun verifyShareScrim() = assertShareScrim()
    fun verifySendToDeviceTitle() = assertSendToDeviceTitle()
    fun verifyShareALinkTitle() = assertShareALinkTitle()
    fun verifyWhatsNewButton() = assertWhatsNewButton()
    fun verifyAddToTopSitesButton() = assertAddToTopSitesButton()
    fun verifyAddToMobileHome() = assertAddToMobileHome()
    fun verifyDesktopSite() = assertDesktopSite()
    fun verifyDownloadsButton() = assertDownloadsButton()
    fun verifyShareTabsOverlay() = assertShareTabsOverlay()
    fun verifySignInToSyncButton() = assertSignInToSyncButton()
    fun verifyNewTabButton() = assertNewTabButton()
    fun verifyReportSiteIssueButton() = assertReportSiteIssueButton()

    fun verifyPageThreeDotMainMenuItems() {
        verifyNewTabButton()
        verifyBookmarksButton()
        verifyAddBookmarkButton()
        verifyHistoryButton()
        verifyDownloadsButton()
        verifyAddOnsButton()
        verifySignInToSyncButton()
        threeDotMenuRecyclerView().perform(swipeUp())
        verifyFindInPageButton()
        verifyDesktopSite()
        threeDotMenuRecyclerView().perform(swipeUp())
        verifyReportSiteIssueButton()
        verifyAddToTopSitesButton()
        verifyAddToMobileHome()
        verifySaveCollection()
        verifySettingsButton()
        verifyShareButton()
        verifyForwardButton()
        verifyRefreshButton()
    }

    private fun assertShareTabsOverlay() {
        onView(withId(R.id.shared_site_list)).check(matches(isDisplayed()))
        onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))
        onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))
        onView(withId(R.id.share_tab_url)).check(matches(isDisplayed()))
    }

    class Transition {

        private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            var maxSwipes = 3
            while (!settingsButton().exists() && maxSwipes != 0) {
                threeDotMenuRecyclerView().perform(swipeUp())
                maxSwipes--
            }
            settingsButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openDownloadsManager(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
            onView(withId(R.id.mozac_browser_menu_recyclerView)).perform(swipeDown())
            downloadsButton().click()

            DownloadRobot().interact()
            return DownloadRobot.Transition()
        }

        fun openSyncSignIn(interact: SyncSignInRobot.() -> Unit): SyncSignInRobot.Transition {
            onView(withId(R.id.mozac_browser_menu_recyclerView)).perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("Sign in to sync")), waitingTime)
            signInToSyncButton().click()

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun openBookmarks(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            onView(withId(R.id.mozac_browser_menu_recyclerView)).perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)

            bookmarksButton().click()
            assertTrue(mDevice.findObject(UiSelector().resourceId("$packageName:id/bookmark_list")).waitForExists(waitingTime))

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun openHistory(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            onView(withId(R.id.mozac_browser_menu_recyclerView)).perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("History")), waitingTime)
            historyButton().click()

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun bookmarkPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)
            addBookmarkButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun sharePage(interact: LibrarySubMenusMultipleSelectionToolbarRobot.() -> Unit): LibrarySubMenusMultipleSelectionToolbarRobot.Transition {
            shareButton().click()
            LibrarySubMenusMultipleSelectionToolbarRobot().interact()
            return LibrarySubMenusMultipleSelectionToolbarRobot.Transition()
        }

        fun openHelp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Help")), waitingTime)
            helpButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goForward(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            forwardButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBack(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            // Close three dot
            mDevice.pressBack()
            // Nav back to previous page
            mDevice.pressBack()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun close(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            // Close three dot
            mDevice.pressBack()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun closeBrowserMenuToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            // Close three dot
            mDevice.pressBack()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun refreshPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            assertRefreshButton()
            refreshButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun stopPageLoad(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.desc("Stop")), waitingTime)
            stopLoadingButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeAllTabs(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            closeAllTabsButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openReportSiteIssue(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            reportSiteIssueButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openFindInPage(interact: FindInPageRobot.() -> Unit): FindInPageRobot.Transition {
            onView(withId(R.id.mozac_browser_menu_recyclerView)).perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("Find in page")), waitingTime)
            findInPageButton().click()

            FindInPageRobot().interact()
            return FindInPageRobot.Transition()
        }

        fun openWhatsNew(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("What’s New")), waitingTime)
            whatsNewButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openReaderViewAppearance(interact: ReaderViewRobot.() -> Unit): ReaderViewRobot.Transition {
            var maxSwipes = 3
            while (!readerViewAppearanceToggle().exists() && maxSwipes != 0) {
                threeDotMenuRecyclerView().perform(swipeUp())
                maxSwipes--
            }
            readerViewAppearanceToggle().click()

            ReaderViewRobot().interact()
            return ReaderViewRobot.Transition()
        }

        fun addToFirefoxHome(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            addToTopSitesButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openAddToHomeScreen(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Add to Home screen")), waitingTime)
            addToHomeScreenButton().click()

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun clickInstall(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            var maxSwipes = 3
            while (!installPWAButton().exists() && maxSwipes != 0) {
                threeDotMenuRecyclerView().perform(swipeUp())
                maxSwipes--
            }
            installPWAButton().click()

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun openSaveToCollection(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            // Ensure the menu is expanded and fully scrolled to the bottom.
            for (i in 0..3) {
                threeDotMenuRecyclerView().perform(swipeUp())
            }

            mDevice.waitNotNull(Until.findObject(By.text("Save to collection")), waitingTime)
            saveCollectionButton().click()
            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }

        fun openAddonsManagerMenu(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            clickAddonsManagerButton()
            mDevice.findObject(
                UiSelector().text("Recommended")
            ).waitForExists(waitingTime)

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }
    }
}
private fun threeDotMenuRecyclerView() =
    onView(withId(R.id.mozac_browser_menu_recyclerView))

private fun threeDotMenuRecyclerViewExists() {
    threeDotMenuRecyclerView().check(matches(isDisplayed()))
}

private fun settingsButton() = mDevice.findObject(UiSelector().text("Settings"))
private fun assertSettingsButton() = assertTrue(settingsButton().waitForExists(waitingTime))

private fun addOnsButton() = onView(allOf(withText("Add-ons")))
private fun assertAddOnsButton() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeDown())
    addOnsButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun historyButton() = onView(allOf(withText(R.string.library_history)))
private fun assertHistoryButton() = historyButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun bookmarksButton() = onView(allOf(withText(R.string.library_bookmarks)))
private fun assertBookmarksButton() = bookmarksButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun signInToSyncButton() = onView(withText("Sign in to sync"))
private fun assertSignInToSyncButton() = signInToSyncButton().check(matches(isDisplayed()))

private fun helpButton() = onView(allOf(withText(R.string.browser_menu_help)))
private fun assertHelpButton() = helpButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun forwardButton() = mDevice.findObject(UiSelector().description("Forward"))
private fun assertForwardButton() = assertTrue(forwardButton().waitForExists(waitingTime))

private fun addBookmarkButton() = onView(allOf(withId(R.id.checkbox), withText("Add")))
private fun assertAddBookmarkButton() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeUp())
    addBookmarkButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun editBookmarkButton() = onView(withText("Edit"))
private fun assertEditBookmarkButton() = editBookmarkButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun refreshButton() = mDevice.findObject(UiSelector().description("Refresh"))
private fun assertRefreshButton() = assertTrue(refreshButton().waitForExists(waitingTime))

private fun stopLoadingButton() = onView(ViewMatchers.withContentDescription("Stop"))

private fun closeAllTabsButton() = onView(allOf(withText("Close all tabs"))).inRoot(RootMatchers.isPlatformPopup())
private fun assertCloseAllTabsButton() = closeAllTabsButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun shareTabButton() = onView(allOf(withText("Share all tabs"))).inRoot(RootMatchers.isPlatformPopup())
private fun assertShareTabButton() = shareTabButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun shareButton() = mDevice.findObject(UiSelector().description("Share"))
private fun assertShareButton() = assertTrue(shareButton().waitForExists(waitingTime))

private fun saveCollectionButton() = onView(allOf(withText("Save to collection"))).inRoot(RootMatchers.isPlatformPopup())
private fun assertSaveCollectionButton() = saveCollectionButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun selectTabsButton() = onView(allOf(withText("Select tabs"))).inRoot(RootMatchers.isPlatformPopup())
private fun assertSelectTabsButton() = selectTabsButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun reportSiteIssueButton() = onView(withText("Report Site Issue…"))
private fun assertReportSiteIssueButton() = reportSiteIssueButton().check(matches(isDisplayed()))

private fun findInPageButton() = onView(allOf(withText("Find in page")))

private fun assertFindInPageButton() = findInPageButton()

private fun shareScrim() = onView(withResourceName("closeSharingScrim"))

private fun assertShareScrim() =
    shareScrim().check(matches(ViewMatchers.withAlpha(ShareFragment.SHOW_PAGE_ALPHA)))

private fun SendToDeviceTitle() =
    onView(allOf(withText("SEND TO DEVICE"), withResourceName("accountHeaderText")))

private fun assertSendToDeviceTitle() = SendToDeviceTitle()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun shareALinkTitle() =
    onView(allOf(withText("ALL ACTIONS"), withResourceName("apps_link_header")))

private fun assertShareALinkTitle() = shareALinkTitle()

private fun whatsNewButton() = onView(
    allOf(
        withText("What’s New"),
        withEffectiveVisibility(Visibility.VISIBLE)
    )
)

private fun assertWhatsNewButton() = whatsNewButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun addToHomeScreenButton() = onView(withText("Add to Home screen"))

private fun readerViewAppearanceToggle() =
    mDevice.findObject(UiSelector().text("Customize reader view"))

private fun assertReaderViewAppearanceButton(visible: Boolean) {
    var maxSwipes = 3
    if (visible) {
        while (!readerViewAppearanceToggle().exists() && maxSwipes != 0) {
            threeDotMenuRecyclerView().perform(swipeUp())
            maxSwipes--
        }
        assertTrue(readerViewAppearanceToggle().exists())
    } else {
        while (!readerViewAppearanceToggle().exists() && maxSwipes != 0) {
            threeDotMenuRecyclerView().perform(swipeUp())
            maxSwipes--
        }
        assertFalse(readerViewAppearanceToggle().exists())
    }
}

private fun addToTopSitesButton() =
    onView(allOf(withText(R.string.browser_menu_add_to_top_sites)))

private fun assertAddToTopSitesButton() {
    onView(withId(R.id.mozac_browser_menu_recyclerView))
        .perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText(R.string.browser_menu_add_to_top_sites))
            )
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun addToMobileHomeButton() =
    onView(allOf(withText(R.string.browser_menu_add_to_homescreen)))

private fun assertAddToMobileHome() {
    onView(withId(R.id.mozac_browser_menu_recyclerView))
        .perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText(R.string.browser_menu_add_to_homescreen))
            )
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun installPWAButton() = mDevice.findObject(UiSelector().text("Install"))

private fun desktopSiteButton() =
    onView(allOf(withText(R.string.browser_menu_desktop_site)))
private fun assertDesktopSite() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeUp())
    desktopSiteButton().check(matches(isDisplayed()))
}

private fun downloadsButton() = onView(withText(R.string.library_downloads))
private fun assertDownloadsButton() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeDown())
    downloadsButton().check(matches(isDisplayed()))
}

private fun clickAddonsManagerButton() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeDown())
    addOnsButton().check(matches(isCompletelyDisplayed())).click()
}

private fun tabSettingsButton() =
    onView(allOf(withText("Tab settings"))).inRoot(RootMatchers.isPlatformPopup())

private fun assertTabSettingsButton() {
    tabSettingsButton()
        .check(
            matches(isDisplayed()))
}

private fun recentlyClosedTabsButton() =
    onView(allOf(withText("Recently closed tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun assertRecentlyClosedTabsButton() {
    recentlyClosedTabsButton()
        .check(
            matches(isDisplayed()))
}

private fun shareAllTabsButton() =
    onView(allOf(withText("Share all tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun assertShareAllTabsButton() {
    shareAllTabsButton()
        .check(
            matches(isDisplayed()))
}

private fun assertNewTabButton() = onView(withText("New tab")).check(matches(isDisplayed()))
