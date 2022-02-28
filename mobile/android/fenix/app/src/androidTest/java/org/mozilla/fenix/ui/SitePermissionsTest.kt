/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying site permissions prompts & functionality
 *
 */
class SitePermissionsTest {
    /* Test page created and handled by the Mozilla mobile test-eng team */
    private val testPage = "https://mozilla-mobile.github.io/testapp/permissions"
    private val testPageSubstring = "https://mozilla-mobile.github.io:443"

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule()

    @SmokeTest
    @Test
    fun audioVideoPermissionChoiceOnEachRequestTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartAudioVideoButton {
            // allow app to record video
            clickAppPermissionButton(true)
            // allow app to record audio
            clickAppPermissionButton(true)
            verifyAudioVideoPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("Camera and Microphone not allowed")
        }.clickStartAudioVideoButton {
        }.clickPagePermissionButton(true) {
            verifyPageContent("Camera and Microphone allowed")
        }
    }

    @SmokeTest
    @Test
    fun rememberBlockAudioVideoPermissionChoiceTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartAudioVideoButton {
            // allow app to record video
            clickAppPermissionButton(true)
            // allow app to record audio
            clickAppPermissionButton(true)
            verifyAudioVideoPermissionPrompt(testPageSubstring)
            selectRememberPermissionDecision()
        }.clickPagePermissionButton(false) {
            verifyPageContent("Camera and Microphone not allowed")
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
        }.clickStartAudioVideoButton { }
        browserScreen {
            verifyPageContent("Camera and Microphone not allowed")
        }
    }

    @Ignore("Failing, see https://github.com/mozilla-mobile/fenix/issues/23358")
    @SmokeTest
    @Test
    fun rememberAllowAudioVideoPermissionChoiceTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartAudioVideoButton {
            // allow app to record video
            clickAppPermissionButton(true)
            // allow app to record audio
            clickAppPermissionButton(true)
            verifyAudioVideoPermissionPrompt(testPageSubstring)
            selectRememberPermissionDecision()
        }.clickPagePermissionButton(true) {
            verifyPageContent("Camera and Microphone allowed")
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
        }.clickStartAudioVideoButton { }
        browserScreen {
            verifyPageContent("Camera and Microphone allowed")
        }
    }

    @SmokeTest
    @Test
    fun blockAppUsingAudioVideoTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickStartAudioVideoButton {
            // allow app to record video
            clickAppPermissionButton(false)
            // allow app to record audio
            clickAppPermissionButton(false)
        }
        browserScreen {
            verifyPageContent("Camera and Microphone not allowed")
        }
    }

    @Test
    fun microphonePermissionChoiceOnEachRequestTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
            clickAppPermissionButton(true)
            verifyMicrophonePermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("Microphone not allowed")
        }.clickStartMicrophoneButton {
        }.clickPagePermissionButton(true) {
            verifyPageContent("Microphone allowed")
        }
    }

    @Test
    fun rememberBlockMicrophonePermissionChoiceTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
            clickAppPermissionButton(true)
            verifyMicrophonePermissionPrompt(testPageSubstring)
            selectRememberPermissionDecision()
        }.clickPagePermissionButton(false) {
            verifyPageContent("Microphone not allowed")
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
        }.clickStartMicrophoneButton { }
        browserScreen {
            verifyPageContent("Microphone not allowed")
        }
    }

    @Ignore("Flaky, needs investigation: https://github.com/mozilla-mobile/fenix/issues/23298")
    @Test
    fun rememberAllowMicrophonePermissionChoiceTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
            clickAppPermissionButton(true)
            verifyMicrophonePermissionPrompt(testPageSubstring)
            selectRememberPermissionDecision()
        }.clickPagePermissionButton(true) {
            verifyPageContent("Microphone allowed")
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
        }.clickStartMicrophoneButton { }
        browserScreen {
            verifyPageContent("Microphone allowed")
        }
    }

    @Test
    fun blockAppUsingMicrophoneTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickStartMicrophoneButton {
            clickAppPermissionButton(false)
        }
        browserScreen {
            verifyPageContent("Microphone not allowed")
        }
    }

    @Test
    fun cameraPermissionChoiceOnEachRequestTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartCameraButton {
            clickAppPermissionButton(true)
            verifyCameraPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("Camera not allowed")
        }.clickStartCameraButton {
        }.clickPagePermissionButton(true) {
            verifyPageContent("Camera allowed")
        }
    }

    @Test
    fun rememberBlockCameraPermissionChoiceTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartCameraButton {
            clickAppPermissionButton(true)
            verifyCameraPermissionPrompt(testPageSubstring)
            selectRememberPermissionDecision()
        }.clickPagePermissionButton(false) {
            verifyPageContent("Camera not allowed")
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
        }.clickStartCameraButton { }
        browserScreen {
            verifyPageContent("Camera not allowed")
        }
    }

    @Test
    fun rememberAllowCameraPermissionChoiceTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartCameraButton {
            clickAppPermissionButton(true)
            verifyCameraPermissionPrompt(testPageSubstring)
            selectRememberPermissionDecision()
        }.clickPagePermissionButton(true) {
            verifyPageContent("Camera allowed")
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
        }.clickStartCameraButton { }
        browserScreen {
            verifyPageContent("Camera allowed")
        }
    }

    @Test
    fun blockAppUsingCameraTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickStartCameraButton {
            clickAppPermissionButton(false)
        }
        browserScreen {
            verifyPageContent("Camera not allowed")
        }
    }

    @Test
    fun blockNotificationsPermissionPromptTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("Notifications not allowed")
        }.openThreeDotMenu {
        }.refreshPage {
            waitForPageToLoad()
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring, true)
        }
    }

    @Test
    fun allowNotificationsPermissionPromptTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(true) {
            verifyPageContent("Notifications allowed")
        }
    }

    @Ignore("Needs mocking location for Firebase - to do: https://github.com/mozilla-mobile/mobile-test-eng/issues/585")
    @Test
    fun allowLocationPermissionsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickGetLocationButton {
            clickAppPermissionButton(true)
            verifyLocationPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(true) {
            verifyPageContent("longitude")
            verifyPageContent("latitude")
        }
    }

    @Ignore("Needs mocking location for Firebase - to do: https://github.com/mozilla-mobile/mobile-test-eng/issues/585")
    @Test
    fun blockLocationPermissionsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickGetLocationButton {
            clickAppPermissionButton(true)
            verifyLocationPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("User denied geolocation prompt")
        }
    }
}
