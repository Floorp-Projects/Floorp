/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test


import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers
import org.json.JSONObject
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assume.assumeThat
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.util.Callbacks

@RunWith(AndroidJUnit4::class)
@MediumTest
@Suppress("DEPRECATION")
class MediaDelegateTest : BaseSessionTest() {

    private fun requestRecordingPermission(allowAudio: Boolean, allowCamera: Boolean) {

        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            @GeckoSessionTestRule.AssertCalled(count = 1)
            override fun onMediaPermissionRequest(
                    session: GeckoSession, uri: String,
                    video: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    audio: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    callback: GeckoSession.PermissionDelegate.MediaCallback) {
                if (! (allowAudio || allowCamera)) {
                    callback.reject();
                    return;
                }
                var audioDevice: GeckoSession.PermissionDelegate.MediaSource? = null;
                var videoDevice: GeckoSession.PermissionDelegate.MediaSource? = null;
                if (allowAudio) {
                    audioDevice = audio!![0];
                }
                if (allowCamera) {
                    videoDevice = video!![0];
                }

                if (videoDevice != null || audioDevice != null) {
                    callback.grant(videoDevice, audioDevice);
                }
            }

            override fun onAndroidPermissionsRequest(session: GeckoSession,
                                                     permissions: Array<out String>?,
                                                     callback: GeckoSession.PermissionDelegate.Callback) {
                callback.grant()
            }
        })

        mainSession.delegateDuringNextWait(object : Callbacks.MediaDelegate {
            @GeckoSessionTestRule.AssertCalled(count = 1)
            override fun onRecordingStatusChanged(session: GeckoSession,
                                                devices:  Array<org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice>) {
                var audioActive = false
                var cameraActive = false
                for (device in devices) {
                    if (device.type == org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice.Type.MICROPHONE) {
                        audioActive = device.status != org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice.Status.INACTIVE
                    }
                    if (device.type == org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice.Type.CAMERA) {
                        cameraActive = device.status != org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice.Status.INACTIVE
                    }
                }

                assertThat("Camera is ${if (allowCamera) { "active" } else { "inactive" }}",
                        cameraActive, Matchers.equalTo(allowCamera))

                assertThat("Audio is ${if (allowAudio ) { "active" } else { "inactive" }}" ,
                        audioActive, Matchers.equalTo(allowAudio))

            }
        })

        var code: String?
        if (allowAudio && allowCamera) {
            code = """this.stream = window.navigator.mediaDevices.getUserMedia({
                       video: { width: 320, height: 240, frameRate: 10 },
                       audio: true
                   });"""
        } else if (allowAudio) {
            code = """this.stream = window.navigator.mediaDevices.getUserMedia({
                       audio: true,
                   });"""
        } else if (allowCamera) {
            code = """this.stream = window.navigator.mediaDevices.getUserMedia({
                       video: { width: 320, height: 240, frameRate: 10 },
                   });"""
        } else {
            return
        }

        // Stop the stream and check active flag and id
        val isActive = mainSession.waitForJS(
                """$code
                   this.stream.then(stream => {
                     if (!stream.active || stream.id == '') {
                       return false;
                     }

                     return true;
                   })
                """.trimMargin()) as Boolean

        assertThat("Stream should be active and id should not be empty.", isActive,
                Matchers.equalTo(true))
    }

    @Test fun testDeviceRecordingEventAudio() {
        // disable test on debug Bug 1555656
        assumeThat(sessionRule.env.isDebugBuild, Matchers.equalTo(false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<JSONObject>()
        val audioDevice = devices.find { map -> map.getString("kind") == "audioinput" }
        if (audioDevice != null) {
            requestRecordingPermission(allowAudio = true, allowCamera = false);
        }
    }

    @Test fun testDeviceRecordingEventVideo() {
        // TODO: needs bug 1700243
        assumeThat(sessionRule.env.isIsolatedProcess, Matchers.equalTo(false))

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<JSONObject>()

        val videoDevice = devices.find { map -> map.getString("kind") == "videoinput" }
        if (videoDevice != null) {
            requestRecordingPermission(allowAudio = false, allowCamera = true)
        }

    }

    @Test fun testDeviceRecordingEventAudioAndVideo() {
        // disabled test on debug builds Bug 1554189
        assumeThat(sessionRule.env.isDebugBuild, Matchers.equalTo(false))
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<JSONObject>()
        val audioDevice = devices.find { map -> map.getString("kind") == "audioinput" }
        val videoDevice = devices.find { map -> map.getString("kind") == "videoinput" }
        if (audioDevice != null && videoDevice != null) {
            requestRecordingPermission(allowAudio = true, allowCamera = true);
        }
    }
}
