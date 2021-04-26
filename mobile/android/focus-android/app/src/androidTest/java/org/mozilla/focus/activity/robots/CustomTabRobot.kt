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
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName

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
        onView(withId(R.id.tracking_protection_toggle_title)).check(matches(isDisplayed()))
        onView(withId(R.id.add_to_homescreen)).check(matches(isDisplayed()))
        onView(withId(R.id.find_in_page)).check(matches(isDisplayed()))
        onView(withId(R.id.open_select_browser)).check(matches(isDisplayed()))
        onView(withId(R.id.open_in_firefox_focus)).check(matches(isDisplayed()))
        onView(withId(R.id.check_menu_item_checkbox)).check(matches(isDisplayed()))
        onView(withId(R.id.report_site_issue)).check(matches(isDisplayed()))
        onView(withId(R.id.branding)).check(matches(withText("Powered by Firefox Focus")))
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

    class Transition
}

fun customTab(interact: CustomTabRobot.() -> Unit): CustomTabRobot.Transition {
    CustomTabRobot().interact()
    return CustomTabRobot.Transition()
}

private fun actionButton(description: String) = onView(withContentDescription(description))

private val menuButton = onView(withId(R.id.menuView))

private val shareButton = onView(withId(R.id.share))

private fun customMenuItem(description: String) = onView(withText(description))

private val closeCustomTabButton = onView(withId(R.id.customtab_close))

private val openInFocusButton = onView(withId(R.id.open_in_firefox_focus))
