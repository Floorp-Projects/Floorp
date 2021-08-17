/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withSubstring
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource

class CustomTabRobot {
    val progressBar: UiObject =
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/progress")
        )

    fun verifyCustomTabActionButton(buttonDescription: String) {
        actionButton(buttonDescription).check(matches(isDisplayed()))
    }

    fun verifyCustomMenuItem(buttonDescription: String) {
        customMenuItem(buttonDescription).check(matches(isDisplayed()))
    }

    fun openCustomTabMenu(): ViewInteraction = menuButton.perform(click())

    fun verifyShareButtonIsDisplayed(): ViewInteraction = shareButton.check(matches(isDisplayed()))

    fun verifyTheStandardMenuItems() {
        onView(withText("Add to Home screen")).check(matches(isDisplayed()))
        onView(withText("Find in Page")).check(matches(isDisplayed()))
        onView(withText("Open in…")).check(matches(isDisplayed()))
        openInFocusButton.check(matches(isDisplayed()))
        onView(withSubstring("Desktop site")).check(matches(isDisplayed()))
        // Removed until https://github.com/mozilla-mobile/android-components/issues/10791 is fixed
        // onView(withText("Report site issue")).check(matches(isDisplayed()))
        onView(withText("Powered by $appName")).check(matches(isDisplayed()))
    }

    fun clickOpenInFocusButton() {
        openInFocusButton
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun closeCustomTab() {
        closeCustomTabButton
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun verifyPageURL(expectedText: String) {
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        customTabUrl.waitForExists(TestHelper.webPageLoadwaitingTime)

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                customTabUrl.text.contains(expectedText)
            )
        }
    }

    class Transition
}

fun customTab(interact: CustomTabRobot.() -> Unit): CustomTabRobot.Transition {
    CustomTabRobot().interact()
    return CustomTabRobot.Transition()
}

private fun actionButton(description: String) = onView(withContentDescription(description))

private val menuButton = onView(withId(R.id.mozac_browser_toolbar_menu))

private val shareButton = onView(withContentDescription("Share link"))

private fun customMenuItem(description: String) = onView(withText(description))

private val closeCustomTabButton = onView(withContentDescription(R.string.mozac_feature_customtabs_exit_button))

private val openInFocusButton = onView(withText("Open in $appName"))

private val customTabUrl =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_url_view"))
