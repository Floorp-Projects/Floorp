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
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.MatcherHelper.assertCheckedItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.checkedItemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the three dot (main) menu.
 */
@Suppress("ForbiddenComment")
class ThreeDotMenuMainRobot {
    fun verifyShareAllTabsButton() = assertShareAllTabsButton()
    fun verifySettingsButton() = assertItemContainingTextExists(settingsButton())
    fun verifyHistoryButton() = assertItemContainingTextExists(historyButton)
    fun verifyThreeDotMenuExists() = threeDotMenuRecyclerViewExists()
    fun verifyAddBookmarkButton() = assertItemWithResIdAndTextExists(addBookmarkButton)
    fun verifyEditBookmarkButton() = assertEditBookmarkButton()
    fun verifyCloseAllTabsButton() = assertCloseAllTabsButton()
    fun verifyReaderViewAppearance(visible: Boolean) = assertReaderViewAppearanceButton(visible)

    fun verifyQuitButtonExists() {
        // Need to double swipe the menu, to make this button visible.
        // In case it reaches the end, the second swipe is no-op.
        expandMenu()
        expandMenu()
        assertItemContainingTextExists(itemWithText("Quit"))
    }

    fun expandMenu() {
        onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeUp())
    }

    fun verifyShareTabButton() = assertShareTabButton()
    fun verifySelectTabs() = assertSelectTabsButton()

    fun verifyFindInPageButton() = assertItemContainingTextExists(findInPageButton)
    fun verifyAddToShortcutsButton() = assertItemContainingTextExists(addToShortcutsButton)
    fun verifyRemoveFromShortcutsButton() = assertRemoveFromShortcutsButton()
    fun verifyShareTabsOverlay() = assertShareTabsOverlay()

    fun verifyDesktopSiteModeEnabled(isRequestDesktopSiteEnabled: Boolean) {
        expandMenu()
        assertCheckedItemWithResIdAndTextExists(desktopSiteToggle(isRequestDesktopSiteEnabled))
    }

    fun verifyPageThreeDotMainMenuItems(isRequestDesktopSiteEnabled: Boolean) {
        expandMenu()
        assertItemContainingTextExists(
            normalBrowsingNewTabButton,
            bookmarksButton,
            historyButton,
            downloadsButton,
            addOnsButton,
            syncAndSaveDataButton,
            findInPageButton,
            desktopSiteButton,
            reportSiteIssueButton,
            addToHomeScreenButton,
            addToShortcutsButton,
            saveToCollectionButton,
            settingsButton(),
        )
        assertCheckedItemWithResIdAndTextExists(addBookmarkButton)
        assertCheckedItemWithResIdAndTextExists(desktopSiteToggle(isRequestDesktopSiteEnabled))
        assertItemWithDescriptionExists(
            backButton,
            forwardButton,
            shareButton,
            refreshButton,
        )
    }

    fun verifyHomeThreeDotMainMenuItems(isRequestDesktopSiteEnabled: Boolean) {
        assertItemContainingTextExists(
            bookmarksButton,
            historyButton,
            downloadsButton,
            addOnsButton,
            // Disabled step due to https://github.com/mozilla-mobile/fenix/issues/26788
            // syncAndSaveDataButton,
            desktopSiteButton,
            whatsNewButton,
            helpButton,
            customizeHomeButton,
            settingsButton(),
        )

        assertCheckedItemWithResIdAndTextExists(desktopSiteToggle(isRequestDesktopSiteEnabled))
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
                    "Addon not listed in the Add-ons sub-menu",
                    mDevice.findObject(UiSelector().textContains(addonName)).waitForExists(waitingTime),
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

    fun clickQuit() {
        expandMenu()
        onView(withText("Quit")).click()
    }

    class Transition {
        fun openSettings(
            localizedText: String = getStringResource(R.string.browser_menu_settings),
            interact: SettingsRobot.() -> Unit,
        ): SettingsRobot.Transition {
            // We require one swipe to display the full size 3-dot menu. On smaller devices
            // such as the Pixel 2, we require two swipes to display the "Settings" menu item
            // at the bottom. On larger devices, the second swipe is a no-op.
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            settingsButton(localizedText).click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openDownloadsManager(interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
            downloadsButton.click()

            DownloadRobot().interact()
            return DownloadRobot.Transition()
        }

        fun openSyncSignIn(interact: SyncSignInRobot.() -> Unit): SyncSignInRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("Sync and save data")), waitingTime)
            syncAndSaveDataButton.click()

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun openBookmarks(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)

            bookmarksButton.click()
            assertTrue(mDevice.findObject(UiSelector().resourceId("$packageName:id/bookmark_list")).waitForExists(waitingTime))

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickNewTabButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            normalBrowsingNewTabButton.click()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun openHistory(interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeDown())
            mDevice.waitNotNull(Until.findObject(By.text("History")), waitingTime)
            historyButton.click()

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun bookmarkPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)
            addBookmarkButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun editBookmarkPage(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Bookmarks")), waitingTime)
            editBookmarkButton().click()

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun openHelp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("Help")), waitingTime)
            helpButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openCustomizeHome(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            mDevice.wait(
                Until
                    .findObject(
                        By.textContains("$packageName:id/browser_menu_customize_home_1"),
                    ),
                waitingTime,
            )

            customizeHomeButton.click()

            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/recycler_view"),
            ).waitForExists(waitingTime)

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun goForward(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            forwardButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goToPreviousPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            backButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickShareButton(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            shareButton.click()
            mDevice.waitNotNull(Until.findObject(By.text("ALL ACTIONS")), waitingTime)

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun closeBrowserMenuToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            // Close three dot
            mDevice.pressBack()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun refreshPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            refreshButton.also {
                it.waitForExists(waitingTime)
                it.click()
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun forceRefreshPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.findObject(By.desc(getStringResource(R.string.browser_menu_refresh)))
                .click(LONG_CLICK_DURATION)

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
            reportSiteIssueButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openFindInPage(interact: FindInPageRobot.() -> Unit): FindInPageRobot.Transition {
            threeDotMenuRecyclerView().perform(swipeUp())
            threeDotMenuRecyclerView().perform(swipeUp())
            mDevice.waitNotNull(Until.findObject(By.text("Find in page")), waitingTime)
            findInPageButton.click()

            FindInPageRobot().interact()
            return FindInPageRobot.Transition()
        }

        fun openWhatsNew(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.text("What’s new")), waitingTime)
            whatsNewButton.click()

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
            for (i in 1..RETRY_COUNT) {
                try {
                    addToShortcutsButton.also {
                        it.waitForExists(waitingTime)
                        it.click()
                    }

                    break
                } catch (e: UiObjectNotFoundException) {
                    if (i == RETRY_COUNT) {
                        throw e
                    } else {
                        mDevice.pressBack()
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
            removeFromShortcutsButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openAddToHomeScreen(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            addToHomeScreenButton.clickAndWaitForNewWindow(waitingTime)

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
            saveToCollectionButton.click()
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
            desktopSiteButton.click()

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

private fun editBookmarkButton() = onView(withText("Edit"))
private fun assertEditBookmarkButton() = editBookmarkButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun stopLoadingButton() = onView(ViewMatchers.withContentDescription("Stop"))

private fun closeAllTabsButton() = onView(allOf(withText("Close all tabs"))).inRoot(RootMatchers.isPlatformPopup())
private fun assertCloseAllTabsButton() = closeAllTabsButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun shareTabButton() = onView(allOf(withText("Share all tabs"))).inRoot(RootMatchers.isPlatformPopup())
private fun assertShareTabButton() = shareTabButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun selectTabsButton() = onView(allOf(withText("Select tabs"))).inRoot(RootMatchers.isPlatformPopup())
private fun assertSelectTabsButton() = selectTabsButton()
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

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

private fun removeFromShortcutsButton() =
    onView(allOf(withText(R.string.browser_menu_remove_from_shortcuts)))

private fun assertRemoveFromShortcutsButton() {
    onView(withId(R.id.mozac_browser_menu_recyclerView))
        .perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText(R.string.browser_menu_settings)),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun installPWAButton() = mDevice.findObject(UiSelector().text("Install"))

private fun openInAppButton() =
    onView(
        allOf(
            withText("Open in app"),
            withEffectiveVisibility(Visibility.VISIBLE),
        ),
    )

private fun clickAddonsManagerButton() {
    onView(withId(R.id.mozac_browser_menu_menuView)).perform(swipeDown())
    addOnsButton.click()
}

private fun shareAllTabsButton() =
    onView(allOf(withText("Share all tabs"))).inRoot(RootMatchers.isPlatformPopup())

private fun assertShareAllTabsButton() {
    shareAllTabsButton()
        .check(
            matches(isDisplayed()),
        )
}

private val bookmarksButton =
    itemContainingText(getStringResource(R.string.library_bookmarks))
private val historyButton =
    itemContainingText(getStringResource(R.string.library_history))
private val downloadsButton =
    itemContainingText(getStringResource(R.string.library_downloads))
private val addOnsButton =
    itemContainingText(getStringResource(R.string.browser_menu_add_ons))
private val desktopSiteButton =
    itemContainingText(getStringResource(R.string.browser_menu_desktop_site))
private fun desktopSiteToggle(state: Boolean) =
    checkedItemWithResIdAndText(
        "$packageName:id/switch_widget",
        getStringResource(R.string.browser_menu_desktop_site),
        state,
    )
private val whatsNewButton =
    itemContainingText(getStringResource(R.string.browser_menu_whats_new))
private val helpButton =
    itemContainingText(getStringResource(R.string.browser_menu_help))
private val customizeHomeButton =
    itemContainingText(getStringResource(R.string.browser_menu_customize_home_1))
private fun settingsButton(localizedText: String = getStringResource(R.string.browser_menu_settings)) =
    itemContainingText(localizedText)
private val syncAndSaveDataButton =
    itemContainingText(getStringResource(R.string.sync_menu_sync_and_save_data))
private val normalBrowsingNewTabButton =
    itemContainingText(getStringResource(R.string.library_new_tab))
private val addBookmarkButton =
    itemWithResIdAndText(
        "$packageName:id/checkbox",
        getStringResource(R.string.browser_menu_add),
    )
private val findInPageButton = itemContainingText(getStringResource(R.string.browser_menu_find_in_page))
private val reportSiteIssueButton = itemContainingText("Report Site Issue")
private val addToHomeScreenButton = itemContainingText(getStringResource(R.string.browser_menu_add_to_homescreen))
private val addToShortcutsButton = itemContainingText(getStringResource(R.string.browser_menu_add_to_shortcuts))
private val saveToCollectionButton = itemContainingText(getStringResource(R.string.browser_menu_save_to_collection_2))
private val backButton = itemWithDescription(getStringResource(R.string.browser_menu_back))
private val forwardButton = itemWithDescription(getStringResource(R.string.browser_menu_forward))
private val shareButton = itemWithDescription(getStringResource(R.string.share_button_content_description))
private val refreshButton = itemWithDescription(getStringResource(R.string.browser_menu_refresh))
