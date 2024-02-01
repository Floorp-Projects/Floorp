/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.clearText
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.doesNotExist
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.res
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.containsString
import org.junit.Assert.assertEquals
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the bookmarks menu.
 */
class BookmarksRobot {

    fun verifyBookmarksMenuView() {
        Log.i(TAG, "verifyBookmarksMenuView: Looking for bookmarks view")
        mDevice.findObject(
            UiSelector().text("Bookmarks"),
        ).waitForExists(waitingTime)

        onView(
            allOf(
                withText("Bookmarks"),
                withParent(withId(R.id.navigationToolbar)),
            ),
        ).check(matches(isDisplayed()))
        Log.i(TAG, "verifyBookmarksMenuView: Verified bookmarks view is displayed")
    }

    fun verifyAddFolderButton() {
        addFolderButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyAddFolderButton: Verified add bookmarks folder button is visible")
    }

    fun verifyCloseButton() {
        closeButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCloseButton: Verified close bookmarks section button is visible")
    }

    fun verifyBookmarkFavicon(forUrl: Uri) {
        bookmarkFavicon(forUrl.toString()).check(
            matches(
                withEffectiveVisibility(
                    ViewMatchers.Visibility.VISIBLE,
                ),
            ),
        )
        Log.i(TAG, "verifyBookmarkFavicon: Verified bookmarks favicon for $forUrl is visible")
    }

    fun verifyBookmarkedURL(url: String) {
        bookmarkURL(url).check(matches(isDisplayed()))
        Log.i(TAG, "verifyBookmarkedURL: Verified bookmarks url: $url is displayed")
    }

    fun verifyFolderTitle(title: String) {
        Log.i(TAG, "verifyFolderTitle: Looking for bookmarks folder with title: $title")
        mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTime)
        onView(withText(title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyFolderTitle: Verified bookmarks folder with title: $title is displayed")
    }

    fun verifyBookmarkFolderIsNotCreated(title: String) {
        Log.i(TAG, "verifyBookmarkFolderIsNotCreated: Looking for bookmarks view")
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/bookmarks_wrapper"),
        ).waitForExists(waitingTime)

        assertUIObjectExists(itemContainingText(title), exists = false)
    }

    fun verifyBookmarkTitle(title: String) {
        Log.i(TAG, "verifyBookmarkTitle: Looking for bookmark with title: $title")
        mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTime)
        onView(withText(title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyBookmarkTitle: Verified bookmark with title: $title is displayed")
    }

    fun verifyBookmarkIsDeleted(expectedTitle: String) {
        Log.i(TAG, "verifyBookmarkIsDeleted: Looking for bookmarks view")
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/bookmarks_wrapper"),
        ).waitForExists(waitingTime)

        assertUIObjectExists(
            itemWithResIdContainingText(
                "$packageName:id/title",
                expectedTitle,
            ),
            exists = false,
        )
    }

    fun verifyUndoDeleteSnackBarButton() {
        snackBarUndoButton().check(matches(withText("UNDO")))
        Log.i(TAG, "verifyUndoDeleteSnackBarButton: Verified bookmark deletion undo snack bar button")
    }

    fun verifySnackBarHidden() {
        mDevice.waitNotNull(
            Until.gone(By.text("UNDO")),
            waitingTime,
        )
        Log.i(TAG, "verifySnackBarHidden: Waited until undo snack bar button is gone")
        onView(withId(R.id.snackbar_layout)).check(doesNotExist())
        Log.i(TAG, "verifySnackBarHidden: Verified bookmark snack bar does not exist")
    }

    fun verifyEditBookmarksView() =
        assertUIObjectExists(
            itemWithDescription("Navigate up"),
            itemWithText(getStringResource(R.string.edit_bookmark_fragment_title)),
            itemWithResId("$packageName:id/delete_bookmark_button"),
            itemWithResId("$packageName:id/save_bookmark_button"),
            itemWithResId("$packageName:id/bookmarkNameEdit"),
            itemWithResId("$packageName:id/bookmarkUrlEdit"),
            itemWithResId("$packageName:id/bookmarkParentFolderSelector"),
        )

    fun verifyKeyboardHidden(isExpectedToBeVisible: Boolean) {
        assertEquals(
            isExpectedToBeVisible,
            mDevice
                .executeShellCommand("dumpsys input_method | grep mInputShown")
                .contains("mInputShown=true"),
        )
        Log.i(TAG, "assertKeyboardVisibility: Verified that the keyboard is visible: $isExpectedToBeVisible")
    }

    fun verifyShareOverlay() {
        onView(withId(R.id.shareWrapper)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareOverlay: Verified bookmarks sharing overlay is displayed")
    }

    fun verifyShareBookmarkFavicon() {
        onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareBookmarkFavicon: Verified shared bookmarks favicon is displayed")
    }

    fun verifyShareBookmarkTitle() {
        onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareBookmarkTitle: Verified shared bookmarks title is displayed")
    }

    fun verifyShareBookmarkUrl() {
        onView(withId(R.id.share_tab_url)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyShareBookmarkUrl: Verified shared bookmarks url is displayed")
    }

    fun verifyCurrentFolderTitle(title: String) {
        Log.i(TAG, "verifyCurrentFolderTitle: Looking for bookmark with title: $title")
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/navigationToolbar")
                .textContains(title),
        )
            .waitForExists(waitingTime)

        onView(
            allOf(
                withText(title),
                withParent(withId(R.id.navigationToolbar)),
            ),
        )
            .check(matches(isDisplayed()))
        Log.i(TAG, "verifyCurrentFolderTitle: Verified bookmark with title: $title is displayed")
    }

    fun waitForBookmarksFolderContentToExist(parentFolderName: String, childFolderName: String) {
        Log.i(TAG, "waitForBookmarksFolderContentToExist: Looking for navigation toolbar containing bookmark folder with title: $parentFolderName")
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/navigationToolbar")
                .textContains(parentFolderName),
        )
            .waitForExists(waitingTime)

        mDevice.waitNotNull(Until.findObject(By.text(childFolderName)), waitingTime)
    }

    fun verifySyncSignInButton() {
        syncSignInButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifySyncSignInButton: Verified sign in to sync button is visible")
    }

    fun cancelFolderDeletion() {
        onView(withText("CANCEL"))
            .inRoot(RootMatchers.isDialog())
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "cancelFolderDeletion: Clicked \"Cancel\" bookmarks folder deletion dialog button")
    }

    fun createFolder(name: String, parent: String? = null) {
        clickAddFolderButton()
        addNewFolderName(name)
        if (!parent.isNullOrBlank()) {
            setParentFolder(parent)
        }
        saveNewFolder()
    }

    fun setParentFolder(parentName: String) {
        clickParentFolderSelector()
        selectFolder(parentName)
        navigateUp()
    }

    fun clickAddFolderButton() {
        mDevice.waitNotNull(
            Until.findObject(By.desc("Add folder")),
            waitingTime,
        )
        addFolderButton().click()
        Log.i(TAG, "clickAddFolderButton: Clicked add bookmarks folder button")
    }

    fun clickAddNewFolderButtonFromSelectFolderView() {
        itemWithResId("$packageName:id/add_folder_button")
            .also {
                it.waitForExists(waitingTime)
                it.click()
            }
        Log.i(TAG, "clickAddNewFolderButtonFromSelectFolderView: Clicked add bookmarks folder button from folder selection view")
    }

    fun addNewFolderName(name: String) {
        addFolderTitleField()
            .click()
            .perform(replaceText(name))
        Log.i(TAG, "addNewFolderName: Bookmarks folder name was set to: $name")
    }

    fun saveNewFolder() {
        saveFolderButton().click()
        Log.i(TAG, "saveNewFolder: Clicked save folder button")
    }

    fun navigateUp() {
        goBackButton().click()
        Log.i(TAG, "navigateUp: Clicked navigate up toolbar button")
    }

    fun clickUndoDeleteButton() {
        snackBarUndoButton().click()
        Log.i(TAG, "clickUndoDeleteButton: Clicked undo snack bar button")
    }

    fun changeBookmarkTitle(newTitle: String) {
        bookmarkNameEditBox()
            .perform(clearText())
            .perform(typeText(newTitle))
        Log.i(TAG, "changeBookmarkTitle: Bookmark title was set to: $newTitle")
    }

    fun changeBookmarkUrl(newUrl: String) {
        bookmarkURLEditBox()
            .perform(clearText())
            .perform(typeText(newUrl))
        Log.i(TAG, "changeBookmarkUrl: Bookmark url was set to: $newUrl")
    }

    fun saveEditBookmark() {
        saveBookmarkButton().click()
        Log.i(TAG, "saveEditBookmark: Clicked save bookmark button")
        Log.i(TAG, "saveEditBookmark: Looking for bookmarks list")
        mDevice.findObject(UiSelector().resourceId("org.mozilla.fenix.debug:id/bookmark_list")).waitForExists(waitingTime)
    }

    fun clickParentFolderSelector() {
        bookmarkFolderSelector().click()
        Log.i(TAG, "clickParentFolderSelector: Clicked folder selector")
    }

    fun selectFolder(title: String) {
        onView(withText(title)).click()
        Log.i(TAG, "selectFolder: Selected folder: $title")
    }

    fun longTapDesktopFolder(title: String) {
        onView(withText(title)).perform(longClick())
        Log.i(TAG, "longTapDesktopFolder: Log tapped folder with title: $title")
    }

    fun cancelDeletion() {
        val cancelButton = mDevice.findObject(UiSelector().textContains("CANCEL"))
        Log.i(TAG, "saveEditBookmark: Looking for \"Cancel\" bookmarks deletion button")
        cancelButton.waitForExists(waitingTime)
        cancelButton.click()
        Log.i(TAG, "saveEditBookmark: Clicked \"Cancel\" bookmarks deletion button")
    }

    fun confirmDeletion() {
        onView(withText(R.string.delete_browsing_data_prompt_allow))
            .inRoot(RootMatchers.isDialog())
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "confirmDeletion: Clicked \"Delete\" bookmarks deletion button")
    }

    fun clickDeleteInEditModeButton() {
        deleteInEditModeButton().click()
        Log.i(TAG, "clickDeleteInEditModeButton: Clicked delete bookmarks button while in edit mode")
    }

    class Transition {
        fun closeMenu(interact: HomeScreenRobot.() -> Unit): Transition {
            closeButton().click()
            Log.i(TAG, "closeMenu: Clicked close bookmarks section button")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openThreeDotMenu(bookmark: String, interact: ThreeDotMenuBookmarksRobot.() -> Unit): ThreeDotMenuBookmarksRobot.Transition {
            mDevice.waitNotNull(Until.findObject(res("$packageName:id/overflow_menu")))
            threeDotMenu(bookmark).click()
            Log.i(TAG, "openThreeDotMenu: Clicked three dot button for bookmark item: $bookmark")

            ThreeDotMenuBookmarksRobot().interact()
            return ThreeDotMenuBookmarksRobot.Transition()
        }

        fun clickSingInToSyncButton(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            syncSignInButton().click()
            Log.i(TAG, "clickSingInToSyncButton: Clicked sign in to sync button")

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked go back button")

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBackToBrowserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            goBackButton().click()
            Log.i(TAG, "goBackToBrowserScreen: Clicked go back button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeEditBookmarkSection(interact: BookmarksRobot.() -> Unit): Transition {
            goBackButton().click()
            Log.i(TAG, "goBackToBrowserScreen: Clicked go back button")

            BookmarksRobot().interact()
            return Transition()
        }

        fun openBookmarkWithTitle(bookmarkTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openBookmarkWithTitle: Looking for bookmark with title: $bookmarkTitle")
            itemWithResIdAndText("$packageName:id/title", bookmarkTitle)
                .also {
                    it.waitForExists(waitingTime)
                    it.clickAndWaitForNewWindow(waitingTimeShort)
                }
            Log.i(TAG, "openBookmarkWithTitle: Clicked bookmark with title: $bookmarkTitle")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSearchButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            itemWithResId("$packageName:id/bookmark_search").click()
            Log.i(TAG, "clickSearchButton: Clicked search bookmarks button")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

fun bookmarksMenu(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
    BookmarksRobot().interact()
    return BookmarksRobot.Transition()
}

private fun closeButton() = onView(withId(R.id.close_bookmarks))

private fun goBackButton() = onView(withContentDescription("Navigate up"))

private fun bookmarkFavicon(url: String) = onView(
    allOf(
        withId(R.id.favicon),
        withParent(
            withParent(
                withChild(allOf(withId(R.id.url), withText(url))),
            ),
        ),
    ),
)

private fun bookmarkURL(url: String) = onView(allOf(withId(R.id.url), withText(containsString(url))))

private fun addFolderButton() = onView(withId(R.id.add_bookmark_folder))

private fun addFolderTitleField() = onView(withId(R.id.bookmarkNameEdit))

private fun saveFolderButton() = onView(withId(R.id.confirm_add_folder_button))

private fun threeDotMenu(bookmark: String) = onView(
    allOf(
        withId(R.id.overflow_menu),
        hasSibling(withText(bookmark)),
    ),
)

private fun snackBarText() = onView(withId(R.id.snackbar_text))

private fun snackBarUndoButton() = onView(withId(R.id.snackbar_btn))

private fun bookmarkNameEditBox() = onView(withId(R.id.bookmarkNameEdit))

private fun bookmarkFolderSelector() = onView(withId(R.id.bookmarkParentFolderSelector))

private fun bookmarkURLEditBox() = onView(withId(R.id.bookmarkUrlEdit))

private fun saveBookmarkButton() = onView(withId(R.id.save_bookmark_button))

private fun deleteInEditModeButton() = onView(withId(R.id.delete_bookmark_button))

private fun syncSignInButton() = onView(withId(R.id.bookmark_folders_sign_in))
