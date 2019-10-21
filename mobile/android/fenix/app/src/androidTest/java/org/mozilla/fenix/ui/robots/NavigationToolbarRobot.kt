/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.pressImeActionButton
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the URL toolbar.
 */
class NavigationToolbarRobot {
    class Transition {

        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun enterURLAndEnterToBrowser(url: Uri, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.wait(Until.findObject(By.text("Search or enter address")), waitingTime)
            urlBar().click()
            awesomeBar().perform(replaceText(url.toString()), pressImeActionButton())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.wait(Until.findObject(By.text("Menu")), waitingTime)
            threeDotButton().click()

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openNewTabAndEnterToBrowser(url: Uri, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.wait(Until.findObject(By.descContains("Add tab")), waitingTime)
            newTab().click()
            awesomeBar().perform(replaceText(url.toString()), pressImeActionButton())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun navigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
    NavigationToolbarRobot().interact()
    return NavigationToolbarRobot.Transition()
}

private fun dismissOnboardingButton() = onView(ViewMatchers.withId(R.id.close_onboarding))
private fun urlBar() = onView(ViewMatchers.withId(R.id.toolbar))
private fun awesomeBar() = onView(ViewMatchers.withId(R.id.mozac_browser_toolbar_edit_url_view))
private fun threeDotButton() = onView(ViewMatchers.withContentDescription("Menu"))
private fun newTab() = onView(ViewMatchers.withContentDescription("Add tab"))
