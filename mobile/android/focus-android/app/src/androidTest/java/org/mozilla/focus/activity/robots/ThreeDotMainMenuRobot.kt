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
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import junit.framework.TestCase.assertTrue
import org.hamcrest.Matchers.allOf
import org.junit.Assert.assertFalse
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.progressBar
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.waitingTimeShort

class ThreeDotMainMenuRobot {

    fun verifyShareButtonExists() = assertTrue(shareBtn.exists())

    fun verifyAddToHomeButtonExists() = assertTrue(addToHomeButton.exists())

    fun verifyFindInPageExists() = findInPageButton.check(matches(isDisplayed()))

    fun verifyOpenInButtonExists() = assertTrue(openInBtn.exists())

    fun verifyRequestDesktopSiteExists() = assertTrue(requestDesktopSiteButton.exists())

    fun verifyRequestDesktopSiteIsEnabled(expectedState: Boolean) {
        if (expectedState) {
            assertTrue(requestDesktopSiteButton.isChecked)
        } else {
            assertFalse(requestDesktopSiteButton.isChecked)
        }
    }

    fun verifySettingsButtonExists() = settingsMenuButton().check(matches(isDisplayed()))

    fun verifyReportSiteIssueButtonExists() {
        // Report Site Issue lazily appears, so we need to wait
        val reportSiteIssueButton = mDevice.wait(
            Until.hasObject(
                By.res("$packageName:id/mozac_browser_menu_menuView").hasDescendant(
                    By.text("Report Site Issue…"),
                ),
            ),
            waitingTime,
        )

        assertTrue(reportSiteIssueButton)
    }

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

    fun clickAddToShortcuts() {
        addShortcutButton.waitForExists(waitingTimeShort)
        addShortcutButton.click()
    }

    class Transition {
        fun openSettings(
            localizedText: String = getStringResource(R.string.menu_settings),
            interact: SettingsRobot.() -> Unit,
        ): SettingsRobot.Transition {
            mDevice.findObject(UiSelector().text(localizedText)).waitForExists(waitingTime)
            settingsMenuButton(localizedText)
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
            addToHomeButton.waitForExists(waitingTime)
            addToHomeButton.click()

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun clickHelpPageLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            helpPageMenuLink
                .check(matches(isDisplayed()))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickReloadButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            reloadButton.click()
            progressBar.waitUntilGone(waitingTime)

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickStopLoadingButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            stopLoadingButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openFindInPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            findInPageButton.perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun switchDesktopSiteMode(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            requestDesktopSiteButton.waitForExists(waitingTime)
            requestDesktopSiteButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun pressBack(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            backButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun pressForward(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            forwardButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private fun settingsMenuButton(localizedText: String = "Settings") =
    onView(
        allOf(withText(localizedText), withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)),
    )

private val shareBtn = mDevice.findObject(
    UiSelector()
        .description("Share…"),
)

private val addShortcutButton =
    mDevice.findObject(
        UiSelector()
            .text("Add to Shortcuts"),
    )

private val reloadButton = mDevice.findObject(
    UiSelector()
        .description("Reload website"),
)

private val stopLoadingButton = mDevice.findObject(
    UiSelector()
        .description("Stop loading website"),
)

private val addToHomeButton = mDevice.findObject(
    UiSelector()
        .text("Add to Home screen"),
)

private val findInPageButton = onView(withText("Find in Page"))

private val helpPageMenuLink = onView(withText("Help"))

private val openInBtn = mDevice.findObject(
    UiSelector()
        .text("Open in…"),
)

private val openInDialogTitle = mDevice.findObject(
    UiSelector()
        .text("Open in…"),
)

private val openWithList = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/apps"),
)

private val requestDesktopSiteButton =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/switch_widget"),
    )

private val backButton =
    mDevice.findObject(
        UiSelector()
            .description("Navigate back"),
    )

private val forwardButton =
    mDevice.findObject(
        UiSelector()
            .description("Navigate forward"),
    )
