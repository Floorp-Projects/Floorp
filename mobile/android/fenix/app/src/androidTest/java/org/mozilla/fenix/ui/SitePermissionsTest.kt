/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.Manifest
import android.content.Context
import android.hardware.camera2.CameraManager
import android.media.AudioManager
import android.os.Build
import androidx.core.net.toUri
import androidx.test.filters.SdkSuppress
import androidx.test.rule.GrantPermissionRule
import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.FeatureSettingsHelper
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestHelper.appContext
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
    private val featureSettingsHelper = FeatureSettingsHelper()
    private val cameraManager = appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    private val micManager = appContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @get:Rule
    val grantPermissionRule: GrantPermissionRule = GrantPermissionRule.grant(
        Manifest.permission.RECORD_AUDIO,
        Manifest.permission.CAMERA
    )

    @Before
    fun setUp() {
        // disabling the new homepage pop-up that interferes with the tests.
        featureSettingsHelper.setJumpBackCFREnabled(false)
        featureSettingsHelper.deleteSitePermissions(true)
    }

    @After
    fun tearDown() {
        // Clearing all permission data after each test to avoid overlapping data
        val applicationContext: Context = activityTestRule.activity.applicationContext
        val permissionStorage = PermissionStorage(applicationContext)

        runBlocking {
            permissionStorage.deleteAllSitePermissions()
        }
    }

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @SmokeTest
    @Test
    fun audioVideoPermissionChoiceOnEachRequestTest() {
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartAudioVideoButton {
            verifyAudioVideoPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("Camera and Microphone not allowed")
        }.clickStartAudioVideoButton {
        }.clickPagePermissionButton(true) {
            verifyPageContent("Camera and Microphone allowed")
        }
    }

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @SmokeTest
    @Test
    fun rememberBlockAudioVideoPermissionChoiceTest() {
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())
        assumeTrue(micManager.microphones.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartAudioVideoButton {
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

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @SmokeTest
    @Test
    fun rememberAllowAudioVideoPermissionChoiceTest() {
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())
        assumeTrue(micManager.microphones.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartAudioVideoButton {
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

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @Test
    fun microphonePermissionChoiceOnEachRequestTest() {
        assumeTrue(micManager.microphones.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
            verifyMicrophonePermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("Microphone not allowed")
        }.clickStartMicrophoneButton {
        }.clickPagePermissionButton(true) {
            verifyPageContent("Microphone allowed")
        }
    }

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @Test
    fun rememberBlockMicrophonePermissionChoiceTest() {
        assumeTrue(micManager.microphones.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
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

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @Test
    fun rememberAllowMicrophonePermissionChoiceTest() {
        assumeTrue(micManager.microphones.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartMicrophoneButton {
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

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @Test
    fun cameraPermissionChoiceOnEachRequestTest() {
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartCameraButton {
            verifyCameraPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("Camera not allowed")
        }.clickStartCameraButton {
        }.clickPagePermissionButton(true) {
            verifyPageContent("Camera allowed")
        }
    }

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @Test
    fun rememberBlockCameraPermissionChoiceTest() {
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartCameraButton {
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

    @SdkSuppress(maxSdkVersion = Build.VERSION_CODES.P, codeName = "P")
    @Test
    fun rememberAllowCameraPermissionChoiceTest() {
        assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
            waitForPageToLoad()
        }.clickStartCameraButton {
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
            verifyLocationPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(false) {
            verifyPageContent("User denied geolocation prompt")
        }
    }
}
