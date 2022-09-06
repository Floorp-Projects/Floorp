/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsSitePermissionsRobot {

    fun verifySitePermissionsItems() {
        assertTrue(autoplaySettings.waitForExists(waitingTime))
        assertTrue(autoplayDefaultValue.exists())
        assertTrue(cameraPermissionsSettings.exists())
        assertTrue(cameraDefaultValue.exists())
        assertTrue(locationPermissionsSettings.exists())
        assertTrue(locationDefaultValue.exists())
        assertTrue(microphonePermissionsSettings.exists())
        assertTrue(microphoneDefaultValue.exists())
        assertTrue(notificationPermissionsSettings.exists())
        assertTrue(notificationDefaultValue.exists())
        assertTrue(DRMContentPermissionsSettings.exists())
        assertTrue(DRMContentDefaultValue.exists())
    }

    fun verifyAutoplaySection() {
        assertTrue(autoplayAllowAudioAndVideoOption.exists())
        assertTrue(autoplayBlockAudioOnlyOption.exists())
        assertTrue(recommendedDescription.exists())
        assertBlockAudioOnlyIsChecked()
        assertTrue(blockAudioAndVideoOption.exists())
    }

    fun openAutoPlaySettings() {
        autoplaySettings.waitForExists(waitingTime)
        autoplaySettings.click()
    }

    fun openCameraPermissionsSettings() {
        cameraPermissionsSettings.waitForExists(waitingTime)
        cameraPermissionsSettings.click()
    }

    fun openLocationPermissionsSettings() {
        locationPermissionsSettings.waitForExists(waitingTime)
        locationPermissionsSettings.click()
    }

    fun verifyAskToAllowChecked() {
        askToAllowRadioButton.waitForExists(waitingTime)
        assertTrue(askToAllowRadioButton.isChecked)
    }

    fun verifyPermissionsStateSettings() {
        assertTrue(askToAllowRadioButton.waitForExists(waitingTime))
        assertTrue(blockedRadioButton.waitForExists(waitingTime))
    }

    fun verifyBlockedByAndroidState() {
        assertTrue(blockedByAndroidInfo.waitForExists(waitingTime))
        assertTrue(goToSettingsButton.waitForExists(waitingTime))
    }

    fun selectBlockAudioVideoAutoplay() {
        blockAudioAndVideoOption.waitForExists(waitingTime)
        blockAudioAndVideoOption.click()
    }

    fun selectAllowAudioVideoAutoplay() {
        autoplayAllowAudioAndVideoOption.waitForExists(waitingTime)
        autoplayAllowAudioAndVideoOption.click()
    }

    class Transition
}

private val autoplaySettings =
    mDevice.findObject(
        UiSelector().text(getStringResource(R.string.preference_autoplay)),
    )

private val autoplayDefaultValue =
    mDevice.findObject(
        UiSelector().text(getStringResource(R.string.preference_block_autoplay_audio_only)),
    )

private val autoplayAllowAudioAndVideoOption =
    mDevice.findObject(UiSelector().text(getStringResource(R.string.preference_allow_audio_video_autoplay)))

private val autoplayBlockAudioOnlyOption =
    mDevice.findObject(UiSelector().text(getStringResource(R.string.preference_block_autoplay_audio_only)))

private val recommendedDescription =
    mDevice.findObject(UiSelector().text(getStringResource(R.string.preference_block_autoplay_audio_only_summary)))

private val blockAudioAndVideoOption =
    mDevice.findObject(UiSelector().text(getStringResource(R.string.preference_block_autoplay_audio_video)))

private fun assertBlockAudioOnlyIsChecked() {
    // the childSelector doesn't work anymore, so we are unable to find it by text
    val radioButton =
        mDevice.findObject(
            UiSelector()
                .checkable(true)
                .index(1),
        )
    assertTrue(radioButton.isChecked)
}

private val locationPermissionsSettings =
    mDevice.findObject(UiSelector().text("Location"))

private val locationDefaultValue =
    mDevice.findObject(UiSelector().text("Location"))
        .getFromParent(UiSelector().text("Blocked by Android"))

private val cameraPermissionsSettings =
    mDevice.findObject(UiSelector().text("Camera"))

private val cameraDefaultValue =
    mDevice.findObject(UiSelector().text("Camera"))
        .getFromParent(UiSelector().text("Blocked by Android"))

private val microphonePermissionsSettings =
    mDevice.findObject(UiSelector().text("Microphone"))

private val microphoneDefaultValue =
    mDevice.findObject(UiSelector().text("Microphone"))
        .getFromParent(UiSelector().text("Blocked by Android"))

private val notificationPermissionsSettings =
    mDevice.findObject(UiSelector().text("Notification"))

private val notificationDefaultValue =
    mDevice.findObject(UiSelector().text("Notification"))
        .getFromParent(UiSelector().text("Ask to allow"))

private val DRMContentPermissionsSettings =
    mDevice.findObject(UiSelector().text("DRM-controlled content"))

private val DRMContentDefaultValue =
    mDevice.findObject(UiSelector().text("DRM-controlled content"))
        .getFromParent(UiSelector().text("Ask to allow"))

private val askToAllowRadioButton =
    // the childSelector doesn't work anymore, so we are unable to find it by text
    mDevice.findObject(
        UiSelector()
            .checkable(true)
            .index(0),
    )

private val blockedRadioButton =
    mDevice.findObject(UiSelector().text("Blocked"))
        .getFromParent(UiSelector().className("android.widget.RadioButton"))

private val blockedByAndroidInfo =
    mDevice.findObject(UiSelector().text("Blocked by Android"))

private val goToSettingsButton =
    mDevice.findObject(UiSelector().text("Go to Settings"))
