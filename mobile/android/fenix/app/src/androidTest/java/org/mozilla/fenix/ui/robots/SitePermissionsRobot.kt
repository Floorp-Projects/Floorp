/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.getPermissionAllowID
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

class SitePermissionsRobot {
    fun clickGrantAppPermissionButton() {
        TestHelper.grantPermission()
    }

    fun clickDenyAppPermissionButton() {
        TestHelper.denyPermission()
    }

    fun verifyMicrophonePermissionPrompt(url: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text("Allow $url to use your microphone?"))
                .waitForExists(waitingTime)
        )
        assertTrue(denyPagePermissionButton.text.equals("Don’t allow"))
        assertTrue(allowPagePermissionButton.text.equals("Allow"))
    }

    fun verifyCameraPermissionPrompt(url: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text("Allow $url to use your camera?"))
                .waitForExists(waitingTime)
        )
        assertTrue(denyPagePermissionButton.text.equals("Don’t allow"))
        assertTrue(allowPagePermissionButton.text.equals("Allow"))
    }

    fun verifyAudioVideoPermissionPrompt(url: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text("Allow $url to use your camera and microphone?"))
                .waitForExists(waitingTime)
        )
        assertTrue(denyPagePermissionButton.text.equals("Don’t allow"))
        assertTrue(allowPagePermissionButton.text.equals("Allow"))
    }

    fun verifyLocationPermissionPrompt(url: String) {
        assertTrue(
            mDevice.findObject(UiSelector().text("Allow $url to use your location?"))
                .waitForExists(waitingTime)
        )
        assertTrue(denyPagePermissionButton.text.equals("Don’t allow"))
        assertTrue(allowPagePermissionButton.text.equals("Allow"))
    }

    fun verifyNotificationsPermissionPrompt(url: String, blocked: Boolean = false) {
        if (!blocked) {
            assertTrue(
                mDevice.findObject(UiSelector().text("Allow $url to send notifications?"))
                    .waitForExists(waitingTime)
            )
            assertTrue(denyPagePermissionButton.text.equals("Never"))
            assertTrue(allowPagePermissionButton.text.equals("Always"))
        } else {
            /* if "Never" was selected in a previous step, or if the app is not allowed,
               the Notifications permission prompt won't be displayed anymore */
            assertFalse(
                mDevice.findObject(UiSelector().text("Allow $url to send notifications?"))
                    .exists()
            )
        }
    }

    fun selectRememberPermissionDecision() {
        onView(withId(R.id.do_not_ask_again))
            .check(matches(isDisplayed()))
            .click()
    }

    class Transition {
        fun clickPagePermissionButton(allow: Boolean, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            if (allow) {
                allowPagePermissionButton.waitForExists(waitingTime)
                allowPagePermissionButton.click()
                // sometimes flaky, the prompt is not dismissed, retrying
                if (!allowPagePermissionButton.waitUntilGone(waitingTime)) {
                    allowPagePermissionButton.click()
                }
            } else {
                denyPagePermissionButton.waitForExists(waitingTime)
                denyPagePermissionButton.click()
                // sometimes flaky, the prompt is not dismissed, retrying
                if (!denyPagePermissionButton.waitUntilGone(waitingTime)) {
                    denyPagePermissionButton.click()
                }
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

// App permission prompts buttons
private val allowSystemPermissionButton =
    mDevice.findObject(UiSelector().resourceId(getPermissionAllowID() + ":id/permission_allow_button"))

private val denySystemPermissionButton =
    mDevice.findObject(UiSelector().resourceId(getPermissionAllowID() + ":id/permission_deny_button"))

// Page permission prompts buttons
private val allowPagePermissionButton =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/allow_button"))

private val denyPagePermissionButton =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/deny_button"))
