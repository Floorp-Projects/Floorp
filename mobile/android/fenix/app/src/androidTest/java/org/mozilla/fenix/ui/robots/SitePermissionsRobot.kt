/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertItemTextEquals
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

class SitePermissionsRobot {
    fun verifyMicrophonePermissionPrompt(url: String) {
        try {
            assertUIObjectExists(itemWithText("Allow $url to use your microphone?"))
            assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
            assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
        } catch (e: AssertionError) {
            Log.i(TAG, "verifyMicrophonePermissionPrompt: AssertionError caught, executing fallback methods")
            browserScreen {
            }.openThreeDotMenu {
            }.refreshPage {
            }.clickStartMicrophoneButton {
                assertUIObjectExists(itemWithText("Allow $url to use your microphone?"))
                assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
                assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
            }
        }
    }

    fun verifyCameraPermissionPrompt(url: String) {
        try {
            assertUIObjectExists(itemWithText("Allow $url to use your camera?"))
            assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
            assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
        } catch (e: AssertionError) {
            Log.i(TAG, "verifyCameraPermissionPrompt: AssertionError caught, executing fallback methods")
            browserScreen {
            }.openThreeDotMenu {
            }.refreshPage {
            }.clickStartCameraButton {
                assertUIObjectExists(itemWithText("Allow $url to use your camera?"))
                assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
                assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
            }
        }
    }

    fun verifyAudioVideoPermissionPrompt(url: String) {
        assertUIObjectExists(itemWithText("Allow $url to use your camera and microphone?"))
        assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
        assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
    }

    fun verifyLocationPermissionPrompt(url: String) {
        try {
            assertUIObjectExists(itemWithText("Allow $url to use your location?"))
            assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
            assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
        } catch (e: AssertionError) {
            Log.i(TAG, "verifyLocationPermissionPrompt: AssertionError caught, executing fallback methods")
            browserScreen {
            }.openThreeDotMenu {
            }.refreshPage {
            }.clickGetLocationButton {
                assertUIObjectExists(itemWithText("Allow $url to use your location?"))
                assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
                assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
            }
        }
    }

    fun verifyNotificationsPermissionPrompt(url: String, blocked: Boolean = false) {
        if (!blocked) {
            try {
                assertUIObjectExists(itemWithText("Allow $url to send notifications?"))
                assertItemTextEquals(denyPagePermissionButton(), expectedText = "Never")
                assertItemTextEquals(allowPagePermissionButton(), expectedText = "Always")
            } catch (e: AssertionError) {
                Log.i(TAG, "verifyNotificationsPermissionPrompt: AssertionError caught, executing fallback methods")
                browserScreen {
                }.openThreeDotMenu {
                }.refreshPage {
                }.clickOpenNotificationButton {
                    assertUIObjectExists(itemWithText("Allow $url to send notifications?"))
                    assertItemTextEquals(denyPagePermissionButton(), expectedText = "Never")
                    assertItemTextEquals(allowPagePermissionButton(), expectedText = "Always")
                }
            }
        } else {
            /* if "Never" was selected in a previous step, or if the app is not allowed,
               the Notifications permission prompt won't be displayed anymore */
            assertUIObjectExists(itemWithText("Allow $url to send notifications?"), exists = false)
        }
    }

    fun verifyPersistentStoragePermissionPrompt(url: String) {
        try {
            assertUIObjectExists(itemWithText("Allow $url to store data in persistent storage?"))
            assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
            assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
        } catch (e: AssertionError) {
            Log.i(TAG, "verifyPersistentStoragePermissionPrompt: AssertionError caught, executing fallback methods")
            browserScreen {
            }.openThreeDotMenu {
            }.refreshPage {
            }.clickRequestPersistentStorageAccessButton {
                assertUIObjectExists(itemWithText("Allow $url to store data in persistent storage?"))
                assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
                assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
            }
        }
    }

    fun verifyDRMContentPermissionPrompt(url: String) {
        try {
            assertUIObjectExists(itemWithText("Allow $url to play DRM-controlled content?"))
            assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
            assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
        } catch (e: AssertionError) {
            Log.i(TAG, "verifyDRMContentPermissionPrompt: AssertionError caught, executing fallback methods")
            browserScreen {
            }.openThreeDotMenu {
            }.refreshPage {
            }.clickRequestDRMControlledContentAccessButton {
                assertUIObjectExists(itemWithText("Allow $url to play DRM-controlled content?"))
                assertItemTextEquals(denyPagePermissionButton(), expectedText = "Don’t allow")
                assertItemTextEquals(allowPagePermissionButton(), expectedText = "Allow")
            }
        }
    }

    fun verifyCrossOriginCookiesPermissionPrompt(originSite: String, currentSite: String) {
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Waiting for $waitingTime ms for \"Allow $originSite to use its cookies on $currentSite?\" prompt to exist")
        mDevice.findObject(UiSelector().text("Allow $originSite to use its cookies on $currentSite?"))
            .waitForExists(waitingTime)
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Waited for $waitingTime ms for \"Allow $originSite to use its cookies on $currentSite?\" prompt to exist")
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Trying to verify that the the storage access permission prompt title is displayed")
        onView(ViewMatchers.withText("Allow $originSite to use its cookies on $currentSite?")).check(matches(isDisplayed()))
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Verified that the the storage access permission prompt title is displayed")
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Trying to verify that the storage access permission prompt message is displayed")
        onView(ViewMatchers.withText("You may want to block access if it's not clear why $originSite needs this data.")).check(matches(isDisplayed()))
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Verified that the storage access permission prompt message is displayed")
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Trying to verify that the storage access permission prompt learn more link is displayed")
        onView(ViewMatchers.withText("Learn more")).check(matches(isDisplayed()))
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Verified that the storage access permission prompt learn more link is displayed")
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Trying to verify that the \"Block\" storage access permission prompt button is displayed")
        onView(ViewMatchers.withText("Block")).check(matches(isDisplayed()))
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Verified that the \"Block\" storage access permission prompt button is displayed")
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Trying to verify that the \"Allow\" storage access permission prompt button is displayed")
        onView(ViewMatchers.withText("Allow")).check(matches(isDisplayed()))
        Log.i(TAG, "verifyCrossOriginCookiesPermissionPrompt: Verified that the \"Allow\" storage access permission prompt button is displayed")
    }

    fun selectRememberPermissionDecision() {
        Log.i(TAG, "selectRememberPermissionDecision: Waiting for $waitingTime ms for the \"Remember decision for this site\" check box to exist")
        mDevice.findObject(UiSelector().resourceId("$packageName:id/do_not_ask_again"))
            .waitForExists(waitingTime)
        Log.i(TAG, "selectRememberPermissionDecision: Waited for $waitingTime ms for the \"Remember decision for this site\" check box to exist")
        Log.i(TAG, "selectRememberPermissionDecision: Trying to click the \"Remember decision for this site\" check box")
        onView(withId(R.id.do_not_ask_again))
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "selectRememberPermissionDecision: Clicked the \"Remember decision for this site\" check box")
    }

    class Transition {
        fun clickPagePermissionButton(allow: Boolean, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            if (allow) {
                Log.i(TAG, "clickPagePermissionButton: Waiting for $waitingTime ms for the \"Allow\" prompt button to exist")
                allowPagePermissionButton().waitForExists(waitingTime)
                Log.i(TAG, "clickPagePermissionButton: Waited for $waitingTime ms for the \"Allow\" prompt button to exist")
                Log.i(TAG, "clickPagePermissionButton: Trying to click the \"Allow\" prompt button")
                allowPagePermissionButton().click()
                Log.i(TAG, "clickPagePermissionButton: Clicked the \"Allow\" prompt button")
                // sometimes flaky, the prompt is not dismissed, retrying
                Log.i(TAG, "clickPagePermissionButton: Waiting for $waitingTime ms for the \"Allow\" prompt button to be gone")
                if (!allowPagePermissionButton().waitUntilGone(waitingTime)) {
                    Log.i(TAG, "clickPagePermissionButton: The \"Allow\" prompt button is not gone")
                    Log.i(TAG, "clickPagePermissionButton: Trying to click again the \"Allow\" prompt button")
                    allowPagePermissionButton().click()
                    Log.i(TAG, "clickPagePermissionButton: Clicked again the \"Allow\" prompt button")
                }
                Log.i(TAG, "clickPagePermissionButton: Waited for $waitingTime ms for the \"Allow\" prompt button to be gone")
            } else {
                Log.i(TAG, "clickPagePermissionButton: Waiting for $waitingTime ms for the \"Don’t allow\" prompt button to exist")
                denyPagePermissionButton().waitForExists(waitingTime)
                Log.i(TAG, "clickPagePermissionButton: Waited for $waitingTime ms for the \"Don’t allow\" prompt button to exist")
                Log.i(TAG, "clickPagePermissionButton: Trying to click the \"Don’t allow\" prompt button")
                denyPagePermissionButton().click()
                Log.i(TAG, "clickPagePermissionButton: Clicked the \"Don’t allow\" prompt button")
                Log.i(TAG, "clickPagePermissionButton: Waiting for $waitingTime ms for the \"Don’t allow\" prompt button to be gone")
                // sometimes flaky, the prompt is not dismissed, retrying
                if (!denyPagePermissionButton().waitUntilGone(waitingTime)) {
                    Log.i(TAG, "clickPagePermissionButton: The \"Don’t allow\" prompt button is not gone")
                    Log.i(TAG, "clickPagePermissionButton: Trying to click again the \"Don’t allow\" prompt button")
                    denyPagePermissionButton().click()
                    Log.i(TAG, "clickPagePermissionButton: Clicked again the \"Don’t allow\" prompt button")
                }
                Log.i(TAG, "clickPagePermissionButton: Waited for $waitingTime ms for the \"Don’t allow\" prompt button to be gone")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

// Page permission prompts buttons
private fun allowPagePermissionButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/allow_button"))

private fun denyPagePermissionButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/deny_button"))
