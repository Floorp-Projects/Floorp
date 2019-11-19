/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.containsString
import org.junit.Assert
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the bookmarks menu.
 */
class BookmarksRobot {

    fun verifyBookmarksMenuView() = assertBookmarksView()

    fun verifyEmptyBookmarksList() = assertEmptyBookmarksList()

    fun verifyBookmarkFavicon() = assertBookmarkFavicon()

    fun verifyBookmarkedURL(url: Uri) = assertBookmarkURL(url)

    fun verifyFolderTitle(title: String) = assertFolderTitle(title)

    fun verifyDeleteSnackBarText() = assertDeleteSnackBarText()

    fun verifyCopySnackBarText() = assertCopySnackBarText()

    fun verifyEditBookmarksView() = assertEditBookmarksView()

    fun verifyBookmarkNameEditBox() = assertBookmarkNameEditBox()

    fun verifyBookmarkURLEditBox() = assertBookmarkURLEditBox()

    fun verifyParentFolderSelector() = assertBookmarkFolderSelector()

    fun verifyHomeScreen() = HomeScreenRobot().verifyHomeScreen()

    fun verifyKeyboardHidden() = assertKeyboardVisibility(isExpectedToBeVisible = false)

    fun verifyKeyboardVisible() = assertKeyboardVisibility(isExpectedToBeVisible = true)

    fun clickAddFolderButton() {
        addFolderButton().click()
    }

    fun addNewFolderName(name: String) {
        addFolderTitleField().click()
        addFolderTitleField().perform(typeText(name))
    }

    fun saveNewFolder() {
        saveFolderButton().click()
    }

    fun navigateUp() {
        goBackButton().click()
    }

    class Transition {
        fun goBack(interact: BookmarksRobot.() -> Unit): Transition {
            goBackButton().click()

            BookmarksRobot().interact()
            return Transition()
        }

        fun openThreeDotMenu(interact: ThreeDotMenuBookmarksRobot.() -> Unit): ThreeDotMenuBookmarksRobot.Transition {
            threeDotMenu().click()

            ThreeDotMenuBookmarksRobot().interact()
            return ThreeDotMenuBookmarksRobot.Transition()
        }
    }
}

fun bookmarksMenu(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
    BookmarksRobot().interact()
    return BookmarksRobot.Transition()
}

private fun goBackButton() = onView(withContentDescription("Navigate up"))

private fun bookmarkFavicon() = onView(withId(R.id.favicon))

private fun bookmarkURL() = onView(withId(R.id.url))

private fun folderTitle() = onView(withId(R.id.title))

private fun addFolderButton() = onView(withId(R.id.add_bookmark_folder))

private fun addFolderTitleField() = onView(withId(R.id.bookmarkNameEdit))

private fun saveFolderButton() = onView(withId(R.id.confirm_add_folder_button))

private fun threeDotMenu() = onView(withId(R.id.overflow_menu))

private fun snackBarText() = onView(withId(R.id.snackbar_text))

private fun assertBookmarksView() {
    onView(
        allOf(
            withText("Bookmarks"),
            withParent(withId(R.id.navigationToolbar))
        )
    )
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertEmptyBookmarksList() =
    onView(withId(R.id.bookmarks_empty_view)).check(matches(withText("No bookmarks here")))

private fun assertBookmarkFavicon() = bookmarkFavicon().check(
    matches(
        withEffectiveVisibility(
            ViewMatchers.Visibility.VISIBLE
        )
    )
)

private fun assertBookmarkURL(expectedURL: Uri) = bookmarkURL()
    .check(matches(ViewMatchers.isCompletelyDisplayed()))
    .check(matches(withText(containsString(expectedURL.toString()))))

private fun assertFolderTitle(expectedTitle: String) = folderTitle()
    .check(matches(withText(expectedTitle)))
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertDeleteSnackBarText() =
    snackBarText().check(matches(withText(containsString("Deleted"))))

private fun assertCopySnackBarText() = snackBarText().check(matches(withText("URL copied")))

private fun assertEditBookmarksView() = onView(withText("Edit bookmark"))
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertBookmarkNameEditBox() =
    onView(withId(R.id.bookmarkNameEdit))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertBookmarkFolderSelector() =
    onView(withId(R.id.bookmarkParentFolderSelector))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertBookmarkURLEditBox() =
    onView(withId(R.id.bookmarkUrlEdit))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertKeyboardVisibility(isExpectedToBeVisible: Boolean) =
    Assert.assertEquals(
        isExpectedToBeVisible,
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
            .executeShellCommand("dumpsys input_method | grep mInputShown")
            .contains("mInputShown=true")
    )
