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
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.not
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the three dot (main) menu.
 */
@Suppress("ForbiddenComment")
class ThreeDotMenuMainRobot {
    fun verifyShareAllTabsButton() = assertShareAllTabsButton()
    fun verifySettingsButton() = assertSettingsButton()
    fun verifyCustomizeHomeButton() = assertCustomizeHomeButton()
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

    fun verifyShareTabButton() = assertShareTabButton()
    fun verifySaveCollection() = assertSaveCollectionButton()
    fun verifySelectTabs() = assertSelectTabsButton()

    fun verifyFindInPageButton() = assertFindInPageButton()
    fun verifyWhatsNewButton() = assertWhatsNewButton()
    fun verifyAddToTopSitesButton() = assertAddToTopSitesButton()
    fun verifyAddToMobileHome() = assertAddToMobileHome()
    fun verifyDesktopSite() = assertDesktopSite()
    fun verifyDownloadsButton() = assertDownloadsButton()
    fun verifyShareTabsOverlay() = assertShareTabsOverlay()
    fun verifySignInToSyncButton() = assertSignInToSyncButton()
    fun verifyNewTabButton() = assertNormalBrowsingNewTabButton()
    fun verifyReportSiteIssueButton() = assertReportSiteIssueButton()

    fun verifyDesktopSiteModeEnabled(state: Boolean) {
        expandMenu()
        if (state) {
            desktopSiteButton().check(matches(isChecked()))
        } else desktopSiteButton().check(matches(not(isChecked())))
    }

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

    fun openAddonsSubList() {
        // when there are add-ons installed, there is an overflow Add-ons sub-menu
        // in that case we use this method instead or before openAddonsManagerMenu()
        clickAddonsManagerButton()
    }

    fun verifyAddonAvailableInMainMenu(addonName: String) {
        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(
                    "Addon not listed in the Add-ons menu",
                    mDevice.findObject(UiSelector().text(addonName)).waitForExists(waitingTime)
                )
                break
            } catch (e: AssertionError) {
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

    class Transition {

        private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            // We require one swipe to display the full size 3-dot menu. On smaller devices
            // such as the Pixel 2, we require two swipes to display the "Settings" menu item
            // at the bottom. On larger devices, the second swipe is a no-op.
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            settingsButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openDownloadsManager(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
            downloadsButton().click()

            DownloadRobot().interact()
            return DownloadRobot.Transition()
        }

        fun openSyncSignIn(interact: SyncSignInRobot.() -> Unit): SyncSignInRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("Sign in to sync")), waitingTime)
            signInToSyncButton().click()

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun openBookmarks(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)

            bookmarksButton().click()
            assertTrue(mDevice.findObject(UiSelector().resourceId("$packageName:id/bookmark_list")).waitForExists(waitingTime))

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun openHistory(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
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

        fun openHelp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Help")), waitingTime)
            helpButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openCustomizeHome(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {

            mDevice.wait(
                Until
                    .findObject(
                        By.textContains("$packageName:id/browser_menu_customize_home_1")
                    ),
                waitingTime
            )

            customizeHomeButton().click()

            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/recycler_view")
            ).waitForExists(waitingTime)

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun goForward(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            forwardButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBack(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            backButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            shareButton().click()
            mDevice.waitNotNull(Until.findObject(By.text("ALL ACTIONS")), waitingTime)

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
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

        fun closeAllTabs(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            closeAllTabsButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openReportSiteIssue(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            reportSiteIssueButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openFindInPage(interact: FindInPageRobot.() -> Unit): FindInPageRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            mDevice.waitNotNull(Until.findObject(By.text("Find in page")), waitingTime)
            findInPageButton().click()

            FindInPageRobot().interact()
            return FindInPageRobot.Transition()
        }

        fun openWhatsNew(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("What’s new")), waitingTime)
            whatsNewButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openReaderViewAppearance(interact: ReaderViewRobot.() -> Unit): ReaderViewRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
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
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            installPWAButton().click()

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun openSaveToCollection(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            // Ensure the menu is expanded and fully scrolled to the bottom.
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())

            mDevice.waitNotNull(Until.findObject(By.text("Save to collection")), waitingTime)
            saveCollectionButton().click()
            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }

        fun openAddonsManagerMenu(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            clickAddonsManagerButton()
            mDevice.findObject(UiSelector().resourceId("$packageName:id/add_ons_list"))
                .waitForExists(waitingTimeLong)

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }

        fun clickOpenInApp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            openInAppButton().click()

            mDevice.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun switchDesktopSiteMode(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            desktopSiteButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareAllTabsButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            shareAllTabsButton().click()

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
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

private fun customizeHomeButton() =
    onView(
        allOf(
            withId(R.id.text),
            withText(R.string.browser_menu_customize_home_1)
        )
    )

private fun assertCustomizeHomeButton() =
    customizeHomeButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

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

private fun backButton() = mDevice.findObject(UiSelector().description("Back"))

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

private fun whatsNewButton() = onView(
    allOf(
        withText("What’s new"),
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
    onView(allOf(withText(R.string.browser_menu_add_to_shortcuts)))

private fun assertAddToTopSitesButton() {
    onView(withId(R.id.mozac_browser_menu_recyclerView))
        .perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText(R.string.browser_menu_add_to_shortcuts))
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

private fun desktopSiteButton() = onView(withId(R.id.switch_widget))
private fun assertDesktopSite() {
    threeDotMenuRecyclerView().perform(swipeUp())
    desktopSiteButton().check(matches(isDisplayed()))
}

private fun openInAppButton() =
    onView(
        allOf(
            withText("Open in app"),
            withEffectiveVisibility(Visibility.VISIBLE)
        )
    )

private fun downloadsButton() = onView(withText(R.string.library_downloads))
private fun assertDownloadsButton() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeDown())
    downloadsButton().check(matches(isDisplayed()))
}

private fun clickAddonsManagerButton() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeDown())
    addOnsButton().check(matches(isCompletelyDisplayed())).click()
}

private fun shareAllTabsButton() =
    onView(allOf(withText("Share all tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun assertShareAllTabsButton() {
    shareAllTabsButton()
        .check(
            matches(isDisplayed())
        )
}

private fun assertNormalBrowsingNewTabButton() = onView(withText("New tab")).check(matches(isDisplayed()))
