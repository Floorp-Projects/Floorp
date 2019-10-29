/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

class BrowserRobot {

    fun verifyBrowserScreen() {
        onView(ViewMatchers.withResourceName("browserLayout"))
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun verifyHelpUrl() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        val redirectUrl = "https://support.mozilla.org/"
        mDevice.waitNotNull(Until.findObject(By.res("org.mozilla.fenix.debug:id/mozac_browser_toolbar_url_view")),
            TestAssetHelper.waitingTime)
        onView(withId(R.id.mozac_browser_toolbar_url_view))
            .check(matches(withText(containsString(redirectUrl))))
    }

    fun verifyWhatsNewURL() {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        val redirectUrl = "https://support.mozilla.org/"
        mDevice.waitNotNull(Until.findObject(By.res("org.mozilla.fenix.debug:id/mozac_browser_toolbar_url_view")),
            TestAssetHelper.waitingTime)
        onView(withId(R.id.mozac_browser_toolbar_url_view))
            .check(matches(withText(containsString(redirectUrl))))
    }

    /* Asserts that the text within DOM element with ID="testContent" has the given text, i.e.
    *  document.querySelector('#testContent').innerText == expectedText
    */
    fun verifyPageContent(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(Until.findObject(By.text(expectedText)), TestAssetHelper.waitingTime)
    }

    fun verifyTabCounter(expectedText: String) {
        onView(withId(R.id.counter_text))
            .check((matches(withText(containsString(expectedText)))))
    }

    fun waitForCollectionSavedPopup() {
        mDevice.wait(Until.findObject(By.text("Tab saved!")),
            TestAssetHelper.waitingTime)
    }

    class Transition {
        private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        private fun threeDotButton() = onView(
            CoreMatchers.allOf(
                ViewMatchers.withContentDescription(
                    "Menu"
                )
            )
        )

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.waitForIdle()
            threeDotButton().perform(ViewActions.click())

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {

            navURLBar().click()

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openHomeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.waitForIdle()

            tabsCounter().click()

            mDevice.waitNotNull(
                Until.findObject(By.res("org.mozilla.fenix.debug:id/header_text")),
                TestAssetHelper.waitingTime
            )

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openQuickActionBar(interact: QuickActionBarRobot.() -> Unit): QuickActionBarRobot.Transition {
            mDevice.waitNotNull(Until.gone(By.res("org.mozilla.fenix.debug:id/quick_action_sheet")),
                TestAssetHelper.waitingTime
            )
            quickActionBarHandle().click()

            QuickActionBarRobot().interact()
            return QuickActionBarRobot.Transition()
        }
    }
}

fun browserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
    BrowserRobot().interact()
    return BrowserRobot.Transition()
}

private fun dismissOnboardingButton() = onView(withId(R.id.close_onboarding))
fun dismissTrackingOnboarding() {
    mDevice.wait(Until.findObject(By.res("close_onboarding")), TestAssetHelper.waitingTime)
    dismissOnboardingButton().click()
}

fun navURLBar() = onView(withId(R.id.mozac_browser_toolbar_url_view))

private fun tabsCounter() = onView(withId(R.id.counter_box))
private fun quickActionBarHandle() = onView(withId(R.id.quick_action_sheet_handle))
