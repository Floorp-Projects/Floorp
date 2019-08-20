/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.click

class BrowserRobot {

    fun verifyHelpUrl() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        val redirectUrl = "https://support.mozilla.org/"

        mDevice.waitForIdle()
        onView(withId(R.id.mozac_browser_toolbar_url_view))
            .check(matches(withText(containsString(redirectUrl))))
    }

    /* Asserts that the text within DOM element with ID="testContent" has the given text, i.e.
    *  document.querySelector('#testContent').innerText == expectedText
    */
    fun verifyPageContent(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.wait(Until.findObject(By.res(expectedText)), TestAssetHelper.waitingTime)
    }

    class Transition {
        private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {

            navURLBar().click()

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openHomeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.waitForIdle()

            tabsCounter().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun browserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
    BrowserRobot().interact()
    return BrowserRobot.Transition()
}

fun navURLBar() = onView(withId(R.id.mozac_browser_toolbar_url_view))

private fun tabsCounter() = onView(withContentDescription("Tabs"))
