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
import androidx.test.rule.GrantPermissionRule
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertExternalAppOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.grantSystemPermission
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MockLocationUpdatesRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying site permissions prompts & functionality
 *
 */
class SitePermissionsTest {
    /* Test page created and handled by the Mozilla mobile test-eng team */
    private lateinit var mockWebServer: MockWebServer
    private val testPage = "https://mozilla-mobile.github.io/testapp/permissions"
    private val testPageSubstring = "https://mozilla-mobile.github.io:443"
    private val cameraManager = appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    private val micManager = appContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule(
        isJumpBackInCFREnabled = false,
        isPWAsPromptEnabled = false,
        isTCPCFREnabled = false,
        isDeleteSitePermissionsEnabled = true,
    )

    @get:Rule
    val grantPermissionRule: GrantPermissionRule = GrantPermissionRule.grant(
        Manifest.permission.RECORD_AUDIO,
        Manifest.permission.CAMERA,
        Manifest.permission.ACCESS_COARSE_LOCATION,
    )

    @get: Rule
    val mockLocationUpdatesRule = MockLocationUpdatesRule()

    @get: Rule
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334295
    @SmokeTest
    @Test
    fun audioVideoPermissionWithoutRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334294
    @Test
    fun blockAudioVideoPermissionRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/251388
    @Test
    fun allowAudioVideoPermissionRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334189
    @Test
    fun microphonePermissionWithoutRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334190
    @Test
    fun blockMicrophonePermissionRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/251387
    @Test
    fun allowMicrophonePermissionRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334076
    @Test
    fun cameraPermissionWithoutRememberingDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334077
    @Test
    fun blockCameraPermissionRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/251386
    @Test
    fun allowCameraPermissionRememberingTheDecisionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334074
    @SmokeTest
    @Test
    fun blockNotificationsPermissionTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/251380
    @Test
    fun allowNotificationsPermissionTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(true) {
            verifyPageContent("Notifications allowed")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/251385
    @SmokeTest
    @Test
    fun allowLocationPermissionsTest() {
        mockLocationUpdatesRule.setMockLocation()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.toUri()) {
        }.clickGetLocationButton {
            verifyLocationPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(true) {
            verifyPageContent("${mockLocationUpdatesRule.latitude}")
            verifyPageContent("${mockLocationUpdatesRule.longitude}")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2334075
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2121537
    @SmokeTest
    @Test
    fun fileUploadPermissionTest() {
        val testPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            clickPageObject(itemWithResId("upload_file"))
            grantSystemPermission()
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                assertExternalAppOpens("com.google.android.documentsui")
            } else {
                assertExternalAppOpens("com.android.documentsui")
            }
        }
    }
}
