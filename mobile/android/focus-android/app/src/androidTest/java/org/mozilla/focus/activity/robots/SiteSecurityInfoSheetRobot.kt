/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.not
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SiteSecurityInfoSheetRobot {

    fun verifySiteConnectionInfoIsSecure(isSecure: Boolean) {
        assertTrue(site_security_info.waitForExists(waitingTime))
        assertTrue(site_identity_title.exists())
        assertTrue(site_identity_Icon.exists())
        if (isSecure) {
            assertTrue(site_security_info.text.equals("Connection is secure"))
        } else {
            assertTrue(site_security_info.text.equals("Connection is not secure"))
        }
    }

    fun verifyTrackingProtectionIsEnabled(enabled: Boolean) {
        if (enabled) {
            trackingProtectionSwitch.check(matches(isChecked()))
        } else {
            trackingProtectionSwitch.check(matches(not(isChecked())))
        }
    }

    class Transition {
        fun closeSecurityInfoSheet(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.pressBack()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickTrackingProtectionSwitch(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            trackingProtectionSwitch.perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private val site_security_info = mDevice.findObject(UiSelector().resourceId("$packageName:id/security_info"))

private val site_identity_title =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/site_title"))

private val site_identity_Icon =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/site_favicon"))

private val trackingProtectionSwitch =
    onView(
        allOf(
            withId(R.id.switch_widget),
            hasSibling(withText("Enhanced Tracking Protection")),
        ),
    ).inRoot(isDialog())
