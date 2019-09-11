package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click

class BookmarksRobot {

    fun verifyBookmarksMenuView() = assertBookmarksView()

    class Transition {
        fun goBack(interact: LibraryRobot.() -> Unit): LibraryRobot.Transition {
            goBackButton().click()

            LibraryRobot().interact()
            return LibraryRobot.Transition()
        }
    }
}

fun bookmarksMenu(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
    BookmarksRobot().interact()
    return BookmarksRobot.Transition()
}

private fun assertBookmarksView() {
    onView(allOf(
            withText("Bookmarks"),
            withParent(withId(R.id.navigationToolbar))))
        .check(ViewAssertions.matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun goBackButton() = onView(withContentDescription("Navigate up"))
