/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.graphics.Bitmap
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.BoundedMatcher
import androidx.test.espresso.matcher.ViewMatchers
import junit.framework.AssertionFailedError
import org.hamcrest.CoreMatchers.not
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.TypeSafeMatcher
import org.mozilla.fenix.helpers.matchers.BitmapDrawableMatcher
import androidx.test.espresso.matcher.ViewMatchers.isChecked as espressoIsChecked
import androidx.test.espresso.matcher.ViewMatchers.isEnabled as espressoIsEnabled
import androidx.test.espresso.matcher.ViewMatchers.isSelected as espressoIsSelected

/**
 * The [espressoIsEnabled] function that can also handle disabled state through the boolean argument.
 */
fun isEnabled(isEnabled: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsEnabled(), isEnabled)

/**
 * The [espressoIsChecked] function that can also handle unchecked state through the boolean argument.
 */
fun isChecked(isChecked: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsChecked(), isChecked)

/**
 * The [espressoIsSelected] function that can also handle not selected state through the boolean argument.
 */
fun isSelected(isSelected: Boolean): Matcher<View> = maybeInvertMatcher(espressoIsSelected(), isSelected)

private fun maybeInvertMatcher(matcher: Matcher<View>, useUnmodifiedMatcher: Boolean): Matcher<View> = when {
    useUnmodifiedMatcher -> matcher
    else -> not(matcher)
}

fun withBitmapDrawable(bitmap: Bitmap, name: String): Matcher<View>? = BitmapDrawableMatcher(bitmap, name)

fun nthChildOf(
    parentMatcher: Matcher<View>,
    childPosition: Int,
): Matcher<View> {
    return object : TypeSafeMatcher<View>() {
        override fun describeTo(description: Description) {
            description.appendText("Position is $childPosition")
        }

        public override fun matchesSafely(view: View): Boolean {
            if (view.parent !is ViewGroup) {
                return parentMatcher.matches(view.parent)
            }
            val group = view.parent as ViewGroup
            return parentMatcher.matches(view.parent) && group.getChildAt(childPosition) == view
        }
    }
}

fun ViewInteraction.isVisibleForUser(): Boolean {
    try {
        check(matches(ViewMatchers.isCompletelyDisplayed()))
    } catch (assertionError: AssertionFailedError) {
        return false
    }

    return true
}

fun atPosition(position: Int, itemMatcher: Matcher<View?>): Matcher<View?>? {
    return object : BoundedMatcher<View?, RecyclerView>(
        RecyclerView::class.java,
    ) {
        override fun describeTo(description: Description) {
            description.appendText("has item at position $position: ")
            itemMatcher.describeTo(description)
        }

        override fun matchesSafely(view: RecyclerView): Boolean {
            val viewHolder = view.findViewHolderForAdapterPosition(position)
                ?: // has no item on such position
                return false
            return itemMatcher.matches(viewHolder.itemView)
        }
    }
}
