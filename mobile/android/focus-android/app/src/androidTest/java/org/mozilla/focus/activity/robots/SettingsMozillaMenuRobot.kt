/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource

class SettingsMozillaMenuRobot {
    fun verifyMozillaMenuItems() {
        mozillaSettingsList.waitForExists(waitingTime)
        showTipsSwitch.check(matches(isDisplayed()))
        aboutFocusPageLink.check(matches(isDisplayed()))
        helpPageLink.check(matches(isDisplayed()))
        yourRightsLink.check(matches(isDisplayed()))
        privacyNoticeLink.check(matches(isDisplayed()))
    }

    fun switchHomeScreenTips() {
        showTipsSwitch
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun openAboutPage() {
        aboutFocusPageLink
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun openYourRightsPage() {
        yourRightsLink
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun openPrivacyNotice() {
        privacyNoticeLink
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun openHelpLink() {
        helpPageLink
            .check(matches(isDisplayed()))
            .perform(click())
    }

    fun verifyVersionNumbers() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
        val versionName = packageInfo.versionName
        val gvBuildId = org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID
        val gvVersion = org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION
        val sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                "Expected app version number not found",
                mDevice.findObject(UiSelector().textContains(versionName))
                    .waitForExists(webPageLoadwaitingTime)
            )

            assertTrue(
                "Expected GV version not found",
                mDevice.findObject(UiSelector().textContains(gvVersion))
                    .waitForExists(webPageLoadwaitingTime)
            )

            assertTrue(
                "Expected GV build ID not found",
                mDevice.findObject(UiSelector().textContains(gvBuildId))
                    .waitForExists(webPageLoadwaitingTime)
            )
        }
    }

    class Transition
}

private val mozillaSettingsList =
    UiScrollable(UiSelector().resourceId("${TestHelper.packageName}:id/recycler_view"))

private val showTipsSwitch = onView(withText("Show home screen tips"))

private val aboutFocusPageLink = onView(withText("About Firefox Focus"))

private val helpPageLink = onView(withText("Help"))

private val yourRightsLink = onView(withText("Your Rights"))

private val privacyNoticeLink = onView(withText("Privacy Notice"))
