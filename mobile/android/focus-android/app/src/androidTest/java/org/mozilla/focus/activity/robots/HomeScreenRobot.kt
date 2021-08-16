/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class HomeScreenRobot {

    fun verifyEmptySearchBar() {
        editURLBar.waitForExists(waitingTime)
        assertTrue(editURLBar.text.equals(getStringResource(R.string.urlbar_hint)))
    }

    fun skipFirstRun() = onView(withId(R.id.skip)).perform(click())

    fun verifyOnboardingFirstSlide() {
        assertTrue(firstSlide.waitForExists(waitingTime))
    }

    fun verifyOnboardingSecondSlide() {
        assertTrue(secondSlide.waitForExists(waitingTime))
    }

    fun verifyOnboardingThirdSlide() {
        assertTrue(thirdSlide.waitForExists(waitingTime))
    }

    fun verifyOnboardingLastSlide() {
        assertTrue(lastSlide.waitForExists(waitingTime))
        assertTrue(finishBtn.text == "OK, GOT IT!")
    }

    fun clickOnboardingNextBtn() = nextBtn.click()

    fun clickOnboardingFinishBtn() = finishBtn.click()

    fun verifyHomeScreenTipIsDisplayed(isDisplayed: Boolean) {
        val teaser = getStringResource(R.string.teaser)
        if (isDisplayed) {
            homeScreenTips.waitForExists(waitingTime)
            assertTrue(homeScreenTips.text != teaser)
        } else {
            assertTrue(homeScreenTips.text == teaser)
        }
    }

    class Transition {
        fun openMainMenu(interact: ThreeDotMainMenuRobot.() -> Unit): ThreeDotMainMenuRobot.Transition {
            editURLBar.waitForExists(waitingTime)
            mainMenu
                .check(matches(isDisplayed()))
                .perform(click())

            ThreeDotMainMenuRobot().interact()
            return ThreeDotMainMenuRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

private val editURLBar =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view")
    )

private val mainMenu = onView(withId(R.id.menuView))

/********* First Run Locators  */
private val firstSlide = mDevice.findObject(
    UiSelector()
        .text("Power up your privacy")
)

private val secondSlide = mDevice.findObject(
    UiSelector()
        .text("Your search, your way")
)

private val thirdSlide = mDevice.findObject(
    UiSelector()
        .text("Add shortcuts to your home screen")
)

private val lastSlide = mDevice.findObject(
    UiSelector()
        .text("Make privacy a habit")
)

private val nextBtn = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/next")
        .enabled(true)
)

private val finishBtn = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/finish")
        .enabled(true)
)

private val homeScreenTips =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/homeViewTipsLabel"))
