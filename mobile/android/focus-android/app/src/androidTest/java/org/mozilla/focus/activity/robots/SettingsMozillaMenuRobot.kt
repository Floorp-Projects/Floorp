/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import junit.framework.TestCase.assertTrue
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.hamcrest.Matchers.allOf
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource

class SettingsMozillaMenuRobot {

    private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource

    fun verifyMozillaMenuItems() {
        mozillaSettingsList.waitForExists(waitingTime)
        aboutFocusPageLink.check(matches(isDisplayed()))
        helpPageLink.check(matches(isDisplayed()))
        yourRightsLink.check(matches(isDisplayed()))
        privacyNoticeLink.check(matches(isDisplayed()))
        licenseInfo.check(matches(isDisplayed()))
        librariesUsedLink.check(matches(isDisplayed()))
    }

    fun verifyVersionNumbers() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val packageInfo = context.packageManager.getPackageInfoCompat(context.packageName, 0)
        val versionName = packageInfo.versionName
        val gvBuildId = org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID
        val gvVersion = org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION

        sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                "Expected app version number not found",
                mDevice.findObject(UiSelector().textContains(versionName))
                    .waitForExists(waitingTime),
            )

            assertTrue(
                "Expected GV version not found",
                mDevice.findObject(UiSelector().textContains(gvVersion))
                    .waitForExists(waitingTime),
            )

            assertTrue(
                "Expected GV build ID not found",
                mDevice.findObject(UiSelector().textContains(gvBuildId))
                    .waitForExists(waitingTime),
            )
        }
    }

    fun verifyLibrariesUsedTitle() {
        librariesUsedTitle
            .check(matches(isDisplayed()))
    }

    class Transition {
        fun openAboutPage(interact: SettingsMozillaMenuRobot.() -> Unit): Transition {
            aboutFocusPageLink
                .check(matches(isDisplayed()))
                .perform(click())

            SettingsMozillaMenuRobot().interact()
            return Transition()
        }

        fun openAboutPageLearnMoreLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.findObject(UiSelector().text("Learn more")).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openYourRightsPage(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            yourRightsLink
                .check(matches(isDisplayed()))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openLicenseInformation(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            licenseInfo
                .check(matches(isDisplayed()))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openLibrariesUsedPage(interact: SettingsMozillaMenuRobot.() -> Unit): BrowserRobot.Transition {
            librariesUsedLink
                .check(matches(isDisplayed()))
                .perform(click())

            SettingsMozillaMenuRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openPrivacyNotice(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            privacyNoticeLink
                .check(matches(isDisplayed()))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openHelpLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            helpPageLink
                .check(matches(isDisplayed()))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private val mozillaSettingsList =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))

private val aboutFocusPageLink = onView(withText("About $appName"))

private val helpPageLink =
    onView(
        allOf(
            withText("Help"),
            withParent(
                hasSibling(withId(R.id.icon_frame)),
            ),
        ),
    )

private val yourRightsLink =
    onView(
        allOf(
            withText("Your Rights"),
            withParent(
                hasSibling(withId(R.id.icon_frame)),
            ),
        ),
    )

private val privacyNoticeLink =
    onView(
        allOf(
            withText("Privacy Notice"),
            withParent(
                hasSibling(withId(R.id.icon_frame)),
            ),
        ),
    )

private val licenseInfo =
    onView(
        allOf(
            withText("Licensing information"),
            withParent(
                hasSibling(withId(R.id.icon_frame)),
            ),
        ),
    )

private val librariesUsedLink = onView(withText("Libraries that we use"))
private val librariesUsedTitle = onView(withText("$appName | OSS Libraries"))
