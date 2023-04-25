/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.waitForObjects

/**
 *  Implementation of the robot pattern for Custom tabs
 */
class CustomTabRobot {

    fun verifyDesktopSiteButtonExists() {
        desktopSiteButton().check(matches(isDisplayed()))
    }

    fun verifyFindInPageButtonExists() {
        findInPageButton().check(matches(isDisplayed()))
    }

    fun verifyPoweredByTextIsDisplayed() {
        assertTrue(
            mDevice.findObject(UiSelector().textContains("POWERED BY $appName"))
                .waitForExists(waitingTime),
        )
    }

    fun verifyOpenInBrowserButtonExists() {
        openInBrowserButton().check(matches(isDisplayed()))
    }

    fun verifyBackButtonExists() = assertTrue(backButton().waitForExists(waitingTime))

    fun verifyForwardButtonExists() = assertTrue(forwardButton().waitForExists(waitingTime))

    fun verifyRefreshButtonExists() = assertTrue(refreshButton().waitForExists(waitingTime))

    fun verifyCustomMenuItem(label: String) {
        assertTrue(mDevice.findObject(UiSelector().text(label)).waitForExists(waitingTime))
    }

    fun verifyCustomTabCloseButton() {
        closeButton().check(matches(isDisplayed()))
    }

    fun verifyCustomTabToolbarTitle(title: String) {
        waitForPageToLoad()

        mDevice.waitForObjects(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_toolbar_title_view")
                    .textContains(title),
            )
                .getFromParent(
                    UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_origin_view"),
                ),
            waitingTime,
        )

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_toolbar_title_view")
                    .textContains(title),
            ).waitForExists(waitingTime),
        )
    }

    fun longCLickAndCopyToolbarUrl() {
        mDevice.waitForObjects(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar")),
            waitingTime,
        )
        customTabToolbar().click(LONG_CLICK_DURATION)
        clickContextMenuItem("Copy")
    }

    fun fillAndSubmitLoginCredentials(userName: String, password: String) {
        mDevice.waitForIdle(waitingTime)
        setPageObjectText(itemWithResId("username"), userName)
        setPageObjectText(itemWithResId("password"), password)
        clickPageObject(itemWithResId("submit"))
        mDevice.waitForObjects(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/save_confirm")),
            waitingTime,
        )
    }

    fun waitForPageToLoad() = progressBar.waitUntilGone(waitingTime)

    class Transition {
        fun openMainMenu(interact: CustomTabRobot.() -> Unit): Transition {
            mainMenuButton().waitForExists(waitingTime)
            mainMenuButton().click()

            CustomTabRobot().interact()
            return Transition()
        }

        fun clickOpenInBrowserButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            openInBrowserButton().perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goBackToOnboardingScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.pressBack()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }
    }
}

fun customTabScreen(interact: CustomTabRobot.() -> Unit): CustomTabRobot.Transition {
    CustomTabRobot().interact()
    return CustomTabRobot.Transition()
}

private fun mainMenuButton() = mDevice.findObject(UiSelector().description("Menu"))

private fun desktopSiteButton() = onView(withId(R.id.switch_widget))

private fun findInPageButton() = onView(withText("Find in page"))

private fun openInBrowserButton() = onView(withText("Open in $appName"))

private fun refreshButton() = mDevice.findObject(UiSelector().description("Refresh"))

private fun forwardButton() = mDevice.findObject(UiSelector().description("Forward"))

private fun backButton() = mDevice.findObject(UiSelector().description("Back"))

private fun closeButton() = onView(withContentDescription("Return to previous app"))

private fun customTabToolbar() = mDevice.findObject(By.res("$packageName:id/toolbar"))

private val progressBar =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_progress"),
    )
