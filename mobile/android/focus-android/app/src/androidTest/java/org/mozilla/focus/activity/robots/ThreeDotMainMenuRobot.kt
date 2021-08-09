/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withSubstring
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import org.hamcrest.Matchers.allOf
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class ThreeDotMainMenuRobot {

    fun verifyShareButtonExists() = assertTrue(shareBtn.exists())

    fun verifyAddToHSButtonExists() = assertTrue(addToHSmenuItem.exists())

    fun verifyFindInPageExists() = findInPageButton.check(matches(isDisplayed()))

    fun verifyOpenInButtonExists() = assertTrue(openInBtn.exists())

    fun verifyRequestDesktopSiteExists() = requestDesktopSiteButton.check(matches(isDisplayed()))

    fun verifySettingsButtonExists() = settingsMenuButton.check(matches(isDisplayed()))

    fun verifyReportSiteIssueButtonExists() = reportSiteIssueButton.check(matches(isDisplayed()))

    fun verifyWhatsNewLinkExists() = whatsNewMenuLink.check(matches(isDisplayed()))

    fun verifyHelpPageLinkExists() = helpPageMenuLink.check(matches(isDisplayed()))

    fun clickOpenInOption() {
        openInBtn.waitForExists(waitingTime)
        openInBtn.click()
    }

    fun verifyOpenInDialog() {
        assertTrue(openInDialogTitle.waitForExists(waitingTime))
        assertTrue(openWithList.waitForExists(waitingTime))
    }

    fun clickOpenInChrome() {
        val chromeBrowser = mDevice.findObject(UiSelector().text("Chrome"))
        if (chromeBrowser.exists()) {
            chromeBrowser.click()
        }
    }

    class Transition {
        fun openSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.findObject(UiSelector().text("Settings")).waitForExists(waitingTime)
            settingsMenuButton
                .check(matches(isDisplayed()))
                .perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openShareScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            shareBtn.waitForExists(waitingTime)
            shareBtn.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openAddToHSDialog(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            addToHSmenuItem.waitForExists(waitingTime)
            addToHSmenuItem.click()

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun clickWhatsNewLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            whatsNewMenuLink
                .check(matches(isDisplayed()))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickHelpPageLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            helpPageMenuLink
                .check(matches(isDisplayed()))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private val settingsMenuButton = onView(
    allOf(withText("Settings"), withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
)

private val shareBtn = mDevice.findObject(
    UiSelector()
        .description("Share…")
)

private val addToHSmenuItem = mDevice.findObject(
    UiSelector()
        .text("Add to Home screen")
)

private val findInPageButton = onView(withText("Find in Page"))

private val whatsNewMenuLink = onView(withText("What’s New"))

private val helpPageMenuLink = onView(withText("Help"))

private val openInBtn = mDevice.findObject(
    UiSelector()
        .text("Open in…")
)

private val openInDialogTitle = mDevice.findObject(
    UiSelector()
        .text("Open in…")
)

private val openWithList = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/apps")
)

private val requestDesktopSiteButton = onView(withSubstring("Desktop site"))

private val reportSiteIssueButton = onView(withText("Report Site Issue…"))
