/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test


import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
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
class MediaDelegateXOriginTest : BaseSessionTest() {

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

        var constraints : String?
        if (allowAudio && allowCamera) {
            constraints = """{
                       video: { width: 320, height: 240, frameRate: 10 },
                       audio: true
                   }"""
        } else if (allowAudio) {
            constraints = "{ audio: true }"
        } else if (allowCamera) {
            constraints = "{video: { width: 320, height: 240, frameRate: 10 }}"
        } else {
            return
        }

        val started = mainSession.waitForJS("Start($constraints)") as String
        assertThat("getUserMedia should have succeeded", started, Matchers.equalTo("ok"))

        val stopped = mainSession.waitForJS("Stop()") as Boolean
        assertThat("stream should have been stopped", stopped, Matchers.equalTo(true))
    }

    private fun requestRecordingPermissionNoAllow(allowAudio: Boolean, allowCamera: Boolean) {

        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            @GeckoSessionTestRule.AssertCalled(count = 0)
            override fun onMediaPermissionRequest(
                    session: GeckoSession, uri: String,
                    video: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    audio: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    callback: GeckoSession.PermissionDelegate.MediaCallback) {
                callback.reject()
            }

            @GeckoSessionTestRule.AssertCalled(count = 0)
            override fun onAndroidPermissionsRequest(session: GeckoSession,
                                                     permissions: Array<out String>?,
                                                     callback: GeckoSession.PermissionDelegate.Callback) {
                callback.reject()
            }
        })

        mainSession.delegateDuringNextWait(object : Callbacks.MediaDelegate {
            @GeckoSessionTestRule.AssertCalled(count = 0)
            override fun onRecordingStatusChanged(session: GeckoSession,
                                                devices:  Array<org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice>) {}
        })

        var constraints : String?
        if (allowAudio && allowCamera) {
            constraints = """{
                       video: { width: 320, height: 240, frameRate: 10 },
                       audio: true
                   }"""
        } else if (allowAudio) {
            constraints = "{ audio: true }"
        } else if (allowCamera) {
            constraints = "{video: { width: 320, height: 240, frameRate: 10 }}"
        } else {
            return
        }

        val started = mainSession.waitForJS("StartNoAllow($constraints)") as String
        assertThat("getUserMedia should not be allowed", started, Matchers.startsWith("NotAllowedError"))

        val stopped = mainSession.waitForJS("Stop()") as Boolean
        assertThat("stream stop should fail", stopped, Matchers.equalTo(false))
    }

    @Test fun testDeviceRecordingEventAudioAndVideoInXOriginIframe() {
        // TODO: Bug 1648153
        assumeThat(sessionRule.env.isFission, Matchers.equalTo(false))

        // TODO: needs bug 1700243
        assumeThat(sessionRule.env.isIsolatedProcess, Matchers.equalTo(false))

        mainSession.loadTestPath(GETUSERMEDIA_XORIGIN_CONTAINER_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<JSONObject>()
        val audioDevice = devices.find { map -> map.getString("kind") == "audioinput" }
        val videoDevice = devices.find { map -> map.getString("kind") == "videoinput" }
        requestRecordingPermission(allowAudio = audioDevice != null,
                allowCamera = videoDevice != null)
    }

    @Test fun testDeviceRecordingEventAudioAndVideoInXOriginIframeNoAllow() {
        // TODO: Bug 1648153
        assumeThat(sessionRule.env.isFission, Matchers.equalTo(false))

        mainSession.loadTestPath(GETUSERMEDIA_XORIGIN_CONTAINER_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<JSONObject>()
        val audioDevice = devices.find { map -> map.getString("kind") == "audioinput" }
        val videoDevice = devices.find { map -> map.getString("kind") == "videoinput" }
        requestRecordingPermissionNoAllow(allowAudio = audioDevice != null,
                allowCamera = videoDevice != null)
    }
}
