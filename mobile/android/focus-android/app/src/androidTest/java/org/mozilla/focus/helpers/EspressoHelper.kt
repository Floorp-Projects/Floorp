/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.helpers

import android.view.View
import android.view.ViewGroup
import androidx.annotation.StringRes
import androidx.test.espresso.Espresso
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import org.hamcrest.Description
import org.hamcrest.Matcher
import org.hamcrest.Matchers
import org.hamcrest.TypeSafeMatcher
import org.mozilla.focus.R

/**
 * Some convenient methods for testing Focus with espresso.
 */
object EspressoHelper {
    @JvmStatic
    fun openSettings() {
        openMenu()
        Espresso.onView(ViewMatchers.withId(R.id.settings))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
        assertToolbarMatchesText(R.string.menu_settings)
    }

    @JvmStatic
    fun openMenu() {
        Espresso.onView(ViewMatchers.withId(R.id.menuView))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
    }

    fun navigateToWebsite(input: String) {
        Espresso.onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_edit_url_view))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.hasFocus()))
            .perform(ViewActions.replaceText(input), ViewActions.pressImeActionButton())
    }

    @JvmStatic
    fun assertToolbarMatchesText(@StringRes titleResource: Int) {
        Espresso.onView(
            Matchers.allOf(
                ViewMatchers.withClassName(Matchers.endsWith("TextView")),
                ViewMatchers.withParent(ViewMatchers.withId(R.id.toolbar))
            )
        )
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .check(ViewAssertions.matches(ViewMatchers.withText(titleResource)))
    }

    @JvmStatic
    fun onFloatingEraseButton(): ViewInteraction {
        return Espresso.onView(ViewMatchers.withId(R.id.erase))
    }

    @JvmStatic
    fun onFloatingTabsButton(): ViewInteraction {
        return Espresso.onView(ViewMatchers.withId(R.id.tabs))
    }

    @JvmStatic
    fun childAtPosition(
        parentMatcher: Matcher<View?>,
        position: Int
    ): Matcher<View> {
        return object : TypeSafeMatcher<View>() {
            override fun describeTo(description: Description) {
                description.appendText("Child at position $position in parent ")
                parentMatcher.describeTo(description)
            }

            public override fun matchesSafely(view: View): Boolean {
                val parent = view.parent
                return (
                    parent is ViewGroup && parentMatcher.matches(parent) &&
                        view == parent.getChildAt(position)
                    )
            }
        }
    }
}
