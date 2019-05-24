/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test


import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import android.util.Log
import org.hamcrest.Matchers
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.util.Callbacks
import org.mozilla.geckoview.GeckoSession.MediaDelegate.RecordingDevice

@RunWith(AndroidJUnit4::class)
@MediumTest
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
                                                devices:  Array<RecordingDevice>) {
                var audioActive = false
                var cameraActive = false
                for (device in devices) {
                    if (device.type == RecordingDevice.Type.MICROPHONE) {
                        audioActive = device.status != RecordingDevice.Status.INACTIVE
                    }
                    if (device.type == RecordingDevice.Type.CAMERA) {
                        cameraActive = device.status != RecordingDevice.Status.INACTIVE
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
            code = """window.navigator.mediaDevices.getUserMedia({
                       video: { width: 320, height: 240, frameRate: 10 },
                       audio: true
                   })"""
        } else if (allowAudio) {
            code = """window.navigator.mediaDevices.getUserMedia({
                       audio: true,
                   })"""
        } else if (allowCamera) {
            code = """window.navigator.mediaDevices.getUserMedia({
                       video: { width: 320, height: 240, frameRate: 10 },
                   })"""
        } else {
            return
        }

        val stream = mainSession.waitForJS(code)

        assertThat("Stream should be active", stream.asJSMap(),
                Matchers.hasEntry("active", true))
        assertThat("Stream should have ID", stream.asJSMap(),
                Matchers.hasEntry(Matchers.equalTo("id"), Matchers.not(Matchers.isEmptyString())))


    }

    @GeckoSessionTestRule.WithDevToolsAPI
    @Test fun testDeviceRecordingEventAudio() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<Map<String, String>>()
        val audioDevice = devices.find { map -> map["kind"] == "audioinput" }
        if (audioDevice != null) {
            requestRecordingPermission(allowAudio = true, allowCamera = false);
        }
    }

    @GeckoSessionTestRule.WithDevToolsAPI
    @Test fun testDeviceRecordingEventVideo() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<Map<String, String>>()
        val videoDevice = devices.find { map -> map["kind"] == "videoinput" }
        if (videoDevice != null) {
            requestRecordingPermission(allowAudio = false, allowCamera = true);
        }

    }

    @GeckoSessionTestRule.WithDevToolsAPI
    @Test fun testDeviceRecordingEventAudioAndVideo() {
        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.waitForJS(
                "window.navigator.mediaDevices.enumerateDevices()").asJSList<Map<String, String>>()
        val audioDevice = devices.find { map -> map["kind"] == "audioinput" }
        val videoDevice = devices.find { map -> map["kind"] == "videoinput" }
        if(audioDevice != null && videoDevice != null) {
            requestRecordingPermission(allowAudio = true, allowCamera = true);
        }
    }
}