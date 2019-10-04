/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isSelected

/**
 * Implementation of Robot Pattern for the quick action bar.
 */
class QuickActionBarRobot {

    fun verifyAddBookmarkButton() = assertAddBookmarkButton()

    fun verifyEditBookmarkButton() = assertEditBookmarkButton()

    fun clickBookmarkButton() {
        addBookmarkButton().click()
    }

    fun createBookmark(url: Uri) {

        navigationToolbar {
        }.enterURLAndEnterToBrowser(url) {
        }.openQuickActionBar {
            clickBookmarkButton()
        }
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun closeQuickActionBar(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            quickActionBarHandle().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun quickActionBar(interact: QuickActionBarRobot.() -> Unit): QuickActionBarRobot.Transition {
    QuickActionBarRobot().interact()
    return QuickActionBarRobot.Transition()
}

private fun quickActionBarHandle() = onView(withId(R.id.quick_action_sheet_handle))

private fun addBookmarkButton() =
    onView(allOf(withId(R.id.quick_action_bookmark), isSelected(false)))

private fun editBookmarkButton() =
    onView(allOf(withId(R.id.quick_action_bookmark), isSelected(true)))

private fun snackBarText() = onView(withId(R.id.snackbar_text))

private fun assertAddBookmarkButton() = addBookmarkButton()
    .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    .check(matches(withText("Bookmark")))

private fun assertBookmarkSavedSnackBarText() =
    snackBarText().check(matches(withText("Bookmark saved!")))

private fun assertEditBookmarkButton() = editBookmarkButton().check(matches(withEffectiveVisibility(
    ViewMatchers.Visibility.VISIBLE)))
    .check(matches(withText("Edit Bookmark")))
