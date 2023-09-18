/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
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
import org.junit.Assert.assertFalse
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the bookmarks menu.
 */
class BookmarksRobot {

    fun verifyBookmarksMenuView() {
        mDevice.findObject(
            UiSelector().text("Bookmarks"),
        ).waitForExists(waitingTime)

        assertBookmarksView()
    }

    fun verifyAddFolderButton() = assertAddFolderButton()

    fun verifyCloseButton() = assertCloseButton()

    fun verifyDeleteMultipleBookmarksSnackBar() = assertSnackBarText("Bookmarks deleted")

    fun verifyBookmarkFavicon(forUrl: Uri) = assertBookmarkFavicon(forUrl)

    fun verifyBookmarkedURL(url: String) = assertBookmarkURL(url)

    fun verifyFolderTitle(title: String) {
        mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTime)
        assertFolderTitle(title)
    }

    fun verifyBookmarkFolderIsNotCreated(title: String) = assertBookmarkFolderIsNotCreated(title)

    fun verifyBookmarkTitle(title: String) {
        mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTime)
        assertBookmarkTitle(title)
    }

    fun verifyBookmarkIsDeleted(expectedTitle: String) = assertBookmarkIsDeleted(expectedTitle)

    fun verifyDeleteSnackBarText() = assertSnackBarText("Deleted")

    fun verifyUndoDeleteSnackBarButton() = assertUndoDeleteSnackBarButton()

    fun verifySnackBarHidden() {
        mDevice.waitNotNull(
            Until.gone(By.text("UNDO")),
            TestAssetHelper.waitingTime,
        )
        onView(withId(R.id.snackbar_layout)).check(doesNotExist())
    }

    fun verifyCopySnackBarText() = assertSnackBarText("URL copied")

    fun verifyEditBookmarksView() {
        assertItemWithDescriptionExists(itemWithDescription("Navigate up"))
        assertItemContainingTextExists(itemWithText(getStringResource(R.string.edit_bookmark_fragment_title)))
        assertItemWithResIdExists(
            itemWithResId("$packageName:id/delete_bookmark_button"),
            itemWithResId("$packageName:id/save_bookmark_button"),
            itemWithResId("$packageName:id/bookmarkNameEdit"),
            itemWithResId("$packageName:id/bookmarkUrlEdit"),
            itemWithResId("$packageName:id/bookmarkParentFolderSelector"),
        )
    }

    fun verifyKeyboardHidden() = assertKeyboardVisibility(isExpectedToBeVisible = false)

    fun verifyKeyboardVisible() = assertKeyboardVisibility(isExpectedToBeVisible = true)

    fun verifyShareOverlay() = assertShareOverlay()

    fun verifyShareBookmarkFavicon() = assertShareBookmarkFavicon()

    fun verifyShareBookmarkTitle() = assertShareBookmarkTitle()

    fun verifyShareBookmarkUrl() = assertShareBookmarkUrl()

    fun verifySelectDefaultFolderSnackBarText() = assertSnackBarText("Can’t edit default folders")

    fun verifyCurrentFolderTitle(title: String) {
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
    }

    fun waitForBookmarksFolderContentToExist(parentFolderName: String, childFolderName: String) {
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/navigationToolbar")
                .textContains(parentFolderName),
        )
            .waitForExists(waitingTime)

        mDevice.waitNotNull(Until.findObject(By.text(childFolderName)), waitingTime)
    }

    fun verifySyncSignInButton() =
        syncSignInButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

    fun verifyDeleteFolderConfirmationMessage() = assertDeleteFolderConfirmationMessage()

    fun cancelFolderDeletion() {
        onView(withText("CANCEL"))
            .inRoot(RootMatchers.isDialog())
            .check(matches(isDisplayed()))
            .click()
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
            TestAssetHelper.waitingTime,
        )
        addFolderButton().click()
    }

    fun clickAddNewFolderButtonFromSelectFolderView() =
        itemWithResId("$packageName:id/add_folder_button")
            .also {
                it.waitForExists(waitingTime)
                it.click()
            }

    fun addNewFolderName(name: String) {
        addFolderTitleField()
            .click()
            .perform(replaceText(name))
    }

    fun saveNewFolder() {
        saveFolderButton().click()
    }

    fun navigateUp() {
        goBackButton().click()
    }

    fun clickUndoDeleteButton() {
        snackBarUndoButton().click()
    }

    fun changeBookmarkTitle(newTitle: String) {
        bookmarkNameEditBox()
            .perform(clearText())
            .perform(typeText(newTitle))
    }

    fun changeBookmarkUrl(newUrl: String) {
        bookmarkURLEditBox()
            .perform(clearText())
            .perform(typeText(newUrl))
    }

    fun saveEditBookmark() {
        saveBookmarkButton().click()
        mDevice.findObject(UiSelector().resourceId("org.mozilla.fenix.debug:id/bookmark_list")).waitForExists(waitingTime)
    }

    fun clickParentFolderSelector() = bookmarkFolderSelector().click()

    fun selectFolder(title: String) = onView(withText(title)).click()

    fun longTapDesktopFolder(title: String) = onView(withText(title)).perform(longClick())

    fun cancelDeletion() {
        val cancelButton = mDevice.findObject(UiSelector().textContains("CANCEL"))
        cancelButton.waitForExists(waitingTime)
        cancelButton.click()
    }

    fun confirmDeletion() {
        onView(withText(R.string.delete_browsing_data_prompt_allow))
            .inRoot(RootMatchers.isDialog())
            .check(matches(isDisplayed()))
            .click()
    }

    fun clickDeleteInEditModeButton() = deleteInEditModeButton().click()

    fun searchBookmarkedItem(bookmarkedItem: String) {
        itemWithResId("$packageName:id/mozac_browser_toolbar_edit_url_view").also {
            it.waitForExists(waitingTime)
            it.setText(bookmarkedItem)
        }
        mDevice.waitForWindowUpdate(packageName, waitingTimeShort)
    }

    fun verifySearchedBookmarkExists(bookmarkUrl: String, exists: Boolean = true) =
        assertItemContainingTextExists(itemContainingText(bookmarkUrl), exists = exists)

    fun dismissBookmarksSearchBar() = mDevice.pressBack()

    class Transition {
        fun closeMenu(interact: HomeScreenRobot.() -> Unit): Transition {
            closeButton().click()

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openThreeDotMenu(bookmark: String, interact: ThreeDotMenuBookmarksRobot.() -> Unit): ThreeDotMenuBookmarksRobot.Transition {
            mDevice.waitNotNull(Until.findObject(res("$packageName:id/overflow_menu")))
            threeDotMenu(bookmark).click()

            ThreeDotMenuBookmarksRobot().interact()
            return ThreeDotMenuBookmarksRobot.Transition()
        }

        fun clickSingInToSyncButton(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            syncSignInButton().click()

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBackToBrowserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            goBackButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeEditBookmarkSection(interact: BookmarksRobot.() -> Unit): Transition {
            goBackButton().click()

            BookmarksRobot().interact()
            return Transition()
        }

        fun openBookmarkWithTitle(bookmarkTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            itemWithResIdAndText("$packageName:id/title", bookmarkTitle)
                .also {
                    it.waitForExists(waitingTime)
                    it.clickAndWaitForNewWindow(waitingTimeShort)
                }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSearchButton(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            itemWithResId("$packageName:id/bookmark_search").click()

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

private fun assertBookmarksView() {
    onView(
        allOf(
            withText("Bookmarks"),
            withParent(withId(R.id.navigationToolbar)),
        ),
    )
        .check(matches(isDisplayed()))
}

private fun assertAddFolderButton() =
    addFolderButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertCloseButton() = closeButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertEmptyBookmarksList() =
    onView(withId(R.id.bookmarks_empty_view)).check(matches(withText("No bookmarks here")))

private fun assertBookmarkFolderIsNotCreated(title: String) {
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/bookmarks_wrapper"),
    ).waitForExists(waitingTime)

    assertFalse(
        mDevice.findObject(
            UiSelector()
                .textContains(title),
        ).waitForExists(waitingTimeShort),
    )
}

private fun assertBookmarkFavicon(forUrl: Uri) = bookmarkFavicon(forUrl.toString()).check(
    matches(
        withEffectiveVisibility(
            ViewMatchers.Visibility.VISIBLE,
        ),
    ),
)

private fun assertBookmarkURL(expectedURL: String) =
    bookmarkURL(expectedURL).check(matches(isDisplayed()))

private fun assertFolderTitle(expectedTitle: String) =
    onView(withText(expectedTitle)).check(matches(isDisplayed()))

private fun assertBookmarkTitle(expectedTitle: String) =
    onView(withText(expectedTitle)).check(matches(isDisplayed()))

private fun assertBookmarkIsDeleted(expectedTitle: String) {
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/bookmarks_wrapper"),
    ).waitForExists(waitingTime)

    assertFalse(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/title")
                .textContains(expectedTitle),
        ).waitForExists(waitingTimeShort),
    )
}
private fun assertUndoDeleteSnackBarButton() =
    snackBarUndoButton().check(matches(withText("UNDO")))

private fun assertSnackBarText(text: String) =
    snackBarText().check(matches(withText(containsString(text))))

private fun assertKeyboardVisibility(isExpectedToBeVisible: Boolean) =
    assertEquals(
        isExpectedToBeVisible,
        mDevice
            .executeShellCommand("dumpsys input_method | grep mInputShown")
            .contains("mInputShown=true"),
    )

private fun assertShareOverlay() =
    onView(withId(R.id.shareWrapper)).check(matches(isDisplayed()))

private fun assertShareBookmarkTitle() =
    onView(withId(R.id.share_tab_title)).check(matches(isDisplayed()))

private fun assertShareBookmarkFavicon() =
    onView(withId(R.id.share_tab_favicon)).check(matches(isDisplayed()))

private fun assertShareBookmarkUrl() =
    onView(withId(R.id.share_tab_url)).check(matches(isDisplayed()))

private fun assertDeleteFolderConfirmationMessage() =
    onView(withText(R.string.bookmark_delete_folder_confirmation_dialog))
        .inRoot(RootMatchers.isDialog())
        .check(matches(isDisplayed()))
