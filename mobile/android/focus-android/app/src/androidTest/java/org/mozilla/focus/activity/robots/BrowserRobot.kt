/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.SessionLoadedIdlingResource
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime

class BrowserRobot {

    val progressBar =
        mDevice.findObject(
            UiSelector().resourceId("$appName:id/progress")
        )

    fun verifyPageContent(expectedText: String) {
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        mDevice.findObject(UiSelector().resourceId("$appName:id/webview"))
            .waitForExists(webPageLoadwaitingTime)

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                mDevice.findObject(UiSelector().textContains(expectedText))
                    .waitForExists(webPageLoadwaitingTime)
            )
        }
    }

    fun verifyPageURL(expectedText: String) {
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        browserURLbar.waitForExists(webPageLoadwaitingTime)

        mDevice.findObject(UiSelector().resourceId("$appName:id/webview"))
            .waitForExists(webPageLoadwaitingTime)

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                browserURLbar.text.contains(expectedText, ignoreCase = true)
            )
        }
    }

    class Transition {
        fun openSearchBar(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            browserURLbar.waitForExists(webPageLoadwaitingTime)
            browserURLbar.click()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clearBrowsingData(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            floatingEraseButton.perform(click())

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun browserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
    BrowserRobot().interact()
    return BrowserRobot.Transition()
}

inline fun runWithIdleRes(ir: IdlingResource?, pendingCheck: () -> Unit) {
    try {
        IdlingRegistry.getInstance().register(ir)
        pendingCheck()
    } finally {
        IdlingRegistry.getInstance().unregister(ir)
    }
}

private val browserURLbar = mDevice.findObject(UiSelector().resourceId("$appName:id/display_url"))

private var floatingEraseButton = onView(allOf(withId(R.id.erase), isDisplayed()))
