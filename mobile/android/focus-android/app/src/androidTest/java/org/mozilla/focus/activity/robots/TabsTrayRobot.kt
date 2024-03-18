/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.containsString
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.waitingTimeShort

class TabsTrayRobot {
    fun verifyTabsOrder(vararg tabTitle: String) {
        for (tab in tabTitle.indices) {
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/session_item")
                        .index(tab)
                        .childSelector(UiSelector().textContains(tabTitle[tab])),
                ).waitForExists(waitingTime),
            )
        }
    }

    fun verifyCloseTabButton(tabTitle: String) = closeTabButton(tabTitle).check(matches(isDisplayed()))

    class Transition {
        fun selectTab(tabTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withText(containsString(tabTitle))).perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun closeTab(tabTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            closeTabButton(tabTitle).perform(click())
            // waiting for the tab to be completely gone before trying other actions on the toolbar
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_toolbar_url_view")
                    .textContains(tabTitle),
            ).waitUntilGone(waitingTimeShort)

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private fun closeTabButton(tabTitle: String) =
    onView(
        allOf(
            withId(R.id.close_button),
            hasSibling(withText(containsString(tabTitle))),
        ),
    )
