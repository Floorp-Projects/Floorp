/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class ThreeDotMainMenuRobot {

    fun verifyTrackersToggleExists() {
        onView(withText("Trackers blocked")).check(matches(isDisplayed()))
        onView(withText("Turning this off may fix some site problems")).check(matches(isDisplayed()))
        trackersCount.check(matches(isDisplayed()))
        blockTrackersToggle.check(matches(isChecked()))
    }

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
        assertTrue(openInDialogTitle.exists())
        assertTrue(openWithList.exists())
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
            // If the menu item is not clickable, wait and retry
            while (!addToHSmenuItem.isClickable) {
                mDevice.pressBack()
                threeDotMenuButton.perform(click())
            }
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

private val settingsMenuButton = onView(withId(R.id.settings))

private val shareBtn = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/share")
)

private val threeDotMenuButton = onView(withId(R.id.menuView))

private val addToHSmenuItem = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/add_to_homescreen")
)

private val findInPageButton = onView(withId(R.id.find_in_page))

private val whatsNewMenuLink = onView(withId(R.id.whats_new))

private val helpPageMenuLink = onView(withId(R.id.help))

private val openInBtn = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/open_select_browser")
)

private val openInDialogTitle = mDevice.findObject(
    UiSelector()
        .text("Open in…")
)

private val openWithList = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/apps")
)

private val requestDesktopSiteButton = onView(withText("Request desktop site"))

private val reportSiteIssueButton = onView(withId((R.id.report_site_issue)))

private val trackersCount = onView(withId(R.id.trackers_count))

private val blockTrackersToggle = onView(withId(R.id.blocking_switch))
