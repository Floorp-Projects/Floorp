/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParentIndex
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.Matchers.allOf
import org.mozilla.focus.R

class TabsTrayRobot {
    fun verifyTabsOrder(vararg tabTitle: String) {
        for (tab in tabTitle.indices) {
            onView(withId(R.id.sessions)).check(
                matches(
                    hasDescendant(
                        allOf(
                            hasDescendant(
                                withText(tabTitle[tab])
                            ),
                            withParentIndex(tab)
                        )
                    )
                )
            )
        }
    }

    class Transition {
        fun selectTab(tabTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withText(tabTitle)).perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeTab(tabTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(allOf(withId(R.id.close_button), hasSibling(withText(tabTitle))))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}
