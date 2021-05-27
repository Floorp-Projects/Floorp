/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.RejectedPromiseException
import org.mozilla.geckoview.test.util.Callbacks

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4

import org.hamcrest.Matchers.*
import org.json.JSONArray
import org.junit.Assert.fail
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Ignore

@RunWith(AndroidJUnit4::class)
@MediumTest
class PermissionDelegateTest : BaseSessionTest() {

    private fun hasPermission(permission: String): Boolean {
        if (Build.VERSION.SDK_INT < 23) {
            return true
        }
        return PackageManager.PERMISSION_GRANTED ==
                InstrumentationRegistry.getInstrumentation().targetContext.checkSelfPermission(permission)
    }

    private fun isEmulator(): Boolean {
        return "generic".equals(Build.DEVICE) || Build.DEVICE.startsWith("generic_")
    }

    @Test fun media() {
        // TODO: needs bug 1700243
        assumeThat(sessionRule.env.isIsolatedProcess, equalTo(false))

        assertInAutomationThat("Should have camera permission",
                hasPermission(Manifest.permission.CAMERA), equalTo(true))

        assertInAutomationThat("Should have microphone permission",
                hasPermission(Manifest.permission.RECORD_AUDIO),
                equalTo(true))

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()

        val devices = mainSession.evaluateJS(
                "window.navigator.mediaDevices.enumerateDevices()") as JSONArray

        var hasVideo = false
        var hasAudio = false
        for (i in 0 until devices.length()) {
            if (devices.getJSONObject(i).getString("kind") == "videoinput") {
                hasVideo = true;
            }
            if (devices.getJSONObject(i).getString("kind") == "audioinput") {
                hasAudio = true;
            }
        }

        assertThat("Device list should contain camera device",
                hasVideo, equalTo(true))
        assertThat("Device list should contain microphone device",
                hasAudio, equalTo(true))

        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            @AssertCalled(count = 1)
            override fun onMediaPermissionRequest(
                    session: GeckoSession, uri: String,
                    video: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    audio: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    callback: GeckoSession.PermissionDelegate.MediaCallback) {
                assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
                assertThat("Video source should be valid", video, not(emptyArray()))

                if (isEmulator()) {
                    callback.grant(video!![0], null)
                } else {
                    assertThat("Audio source should be valid", audio, not(emptyArray()))
                    callback.grant(video!![0], audio!![0])
                }
            }
        })

        // Start a video stream, with audio if on a real device.
        var code: String?
        if (isEmulator()) {
            code = """this.stream = window.navigator.mediaDevices.getUserMedia({
                       video: { width: 320, height: 240, frameRate: 10 },
                   });"""
        } else {
            code = """this.stream = window.navigator.mediaDevices.getUserMedia({
                       video: { width: 320, height: 240, frameRate: 10 },
                       audio: true
                   });"""
        }

        // Stop the stream and check active flag and id
        val isActive = mainSession.waitForJS(
                """$code
                   this.stream.then(stream => {
                     if (!stream.active || stream.id == '') {
                       return false;
                     }

                     stream.getTracks().forEach(track => track.stop());
                     return true;
                   })
                """.trimMargin()) as Boolean

        assertThat("Stream should be active and id should not be empty.", isActive, equalTo(true));

        // Now test rejecting the request.
        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            @AssertCalled(count = 1)
            override fun onMediaPermissionRequest(
                    session: GeckoSession, uri: String,
                    video: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    audio: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
                    callback: GeckoSession.PermissionDelegate.MediaCallback) {
                callback.reject()
            }
        })

        try {
            if (isEmulator()) {
                mainSession.waitForJS("""
                        window.navigator.mediaDevices.getUserMedia({ video: true })""")
            } else {
                mainSession.waitForJS("""
                        window.navigator.mediaDevices.getUserMedia({ audio: true: video: true })""")
            }
            fail("Request should have failed")
        } catch (e: RejectedPromiseException) {
            assertThat("Error should be correct",
                    e.reason as String, containsString("NotAllowedError"))
        }
    }

    @Test fun geolocation() {
        assertInAutomationThat("Should have location permission",
                hasPermission(Manifest.permission.ACCESS_FINE_LOCATION),
                equalTo(true))

        val url = createTestUrl(HELLO_HTML_PATH)
        mainSession.loadUri(url)
        mainSession.waitForPageStop()

        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            // Ensure the content permission is asked first, before the Android permission.
            @AssertCalled(count = 1, order = [1])
            override fun onContentPermissionRequest(
                    session: GeckoSession, uri: String?, type: Int,
                    callback: GeckoSession.PermissionDelegate.Callback) {
                assertThat("URI should match", uri, endsWith(url))
                assertThat("Type should match", type,
                        equalTo(GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION))
                callback.grant()
            }

            @AssertCalled(count = 1, order = [2])
            override fun onAndroidPermissionsRequest(
                    session: GeckoSession, permissions: Array<out String>?,
                    callback: GeckoSession.PermissionDelegate.Callback) {
                assertThat("Permissions list should be correct",
                        listOf(*permissions!!), hasItems(Manifest.permission.ACCESS_FINE_LOCATION))
                callback.grant()
            }
        })

        try {
            val hasPosition = mainSession.waitForJS("""new Promise((resolve, reject) =>
                    window.navigator.geolocation.getCurrentPosition(
                        position => resolve(
                            position.coords.latitude !== undefined &&
                            position.coords.longitude !== undefined),
                        error => reject(error.code)))""") as Boolean

            assertThat("Request should succeed", hasPosition, equalTo(true))
        } catch (ex: RejectedPromiseException) {
            assertThat("Error should not because the permission was denied.",
                    ex.reason as String, not("1"))
        }

        val perms = sessionRule.waitForResult(sessionRule.runtime.storageController.getPermissions(url))

        assertThat("Permissions should not be null", perms, notNullValue())
        var permFound : Boolean = false
        for (perm in perms) {
            if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION &&
                    url.startsWith(perm.uri) && perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW) {
                permFound = true
            }
        }

        assertThat("Geolocation permission should be set to allow", permFound, equalTo(true))

        mainSession.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                var permFound2 : Boolean = false
                for (perm in perms) {
                    if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION &&
                            perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW) {
                        permFound2 = true
                    }
                }
                assertThat("Geolocation permission must be present on refresh", permFound2, equalTo(true))
            }
        })
        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @Test fun geolocation_reject() {
        val url = createTestUrl(HELLO_HTML_PATH)
        mainSession.loadUri(url)
        mainSession.waitForPageStop()

        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            @AssertCalled(count = 1)
            override fun onContentPermissionRequest(
                    session: GeckoSession, uri: String?, type: Int,
                    callback: GeckoSession.PermissionDelegate.Callback) {
                callback.reject()
            }

            @AssertCalled(count = 0)
            override fun onAndroidPermissionsRequest(
                    session: GeckoSession, permissions: Array<out String>?,
                    callback: GeckoSession.PermissionDelegate.Callback) {
            }
        })

        val errorCode = mainSession.waitForJS("""new Promise((resolve, reject) =>
                window.navigator.geolocation.getCurrentPosition(reject,
                  error => resolve(error.code)
                ))""")

        // Error code 1 means permission denied.
        assertThat("Request should fail", errorCode as Double, equalTo(1.0))

        val perms = sessionRule.waitForResult(sessionRule.runtime.storageController.getPermissions(url))

        assertThat("Permissions should not be null", perms, notNullValue())
        var permFound : Boolean = false
        for (perm in perms) {
            if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION &&
                    url.startsWith(perm.uri) && perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY) {
                permFound = true
            }
        }

        assertThat("Geolocation permission should be set to allow", permFound, equalTo(true))

        mainSession.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                var permFound2 : Boolean = false
                for (perm in perms) {
                    if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION &&
                            perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY) {
                        permFound2 = true
                    }
                }
                assertThat("Geolocation permission must be present on refresh", permFound2, equalTo(true))
            }
        })
        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @Test fun notification() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.webnotifications.requireuserinteraction" to false))
        val url = createTestUrl(HELLO_HTML_PATH)
        mainSession.loadUri(url)
        mainSession.waitForPageStop()

        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            @AssertCalled(count = 1)
            override fun onContentPermissionRequest(
                    session: GeckoSession, uri: String?, type: Int,
                    callback: GeckoSession.PermissionDelegate.Callback) {
                assertThat("URI should match", uri, endsWith(url))
                assertThat("Type should match", type,
                        equalTo(GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION))
                callback.grant()
            }
        })

        val result = mainSession.waitForJS("Notification.requestPermission()")

        assertThat("Permission should be granted",
                result as String, equalTo("granted"))

        val perms = sessionRule.waitForResult(sessionRule.runtime.storageController.getPermissions(url))

        assertThat("Permissions should not be null", perms, notNullValue())
        var permFound : Boolean = false
        for (perm in perms) {
            if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION &&
                    url.startsWith(perm.uri) && perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW) {
                permFound = true
            }
        }

        assertThat("Notification permission should be set to allow", permFound, equalTo(true))

        mainSession.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                var permFound2 : Boolean = false
                for (perm in perms) {
                    if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION &&
                            perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW) {
                        permFound2 = true
                    }
                }
                assertThat("Notification permission must be present on refresh", permFound2, equalTo(true))
            }
        })
        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @Ignore("disable test for frequently failing Bug 1542525")
    @Test fun notification_reject() {
        val url = createTestUrl(HELLO_HTML_PATH)
        mainSession.loadUri(url)
        mainSession.waitForPageStop()

        mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
            @AssertCalled(count = 1)
            override fun onContentPermissionRequest(
                    session: GeckoSession, uri: String?, type: Int,
                    callback: GeckoSession.PermissionDelegate.Callback) {
                callback.reject()
            }
        })

        val result = mainSession.waitForJS("Notification.requestPermission()")

        assertThat("Permission should not be granted",
                result as String, equalTo("denied"))

        val perms = sessionRule.waitForResult(sessionRule.runtime.storageController.getPermissions(url))

        assertThat("Permissions should not be null", perms, notNullValue())
        var permFound : Boolean = false
        for (perm in perms) {
            if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION &&
                    url.startsWith(perm.uri) && perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY) {
                permFound = true
            }
        }

        assertThat("Notification permission should be set to allow", permFound, equalTo(true))

        mainSession.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String?, perms: MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                var permFound2 : Boolean = false
                for (perm in perms) {
                    if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION &&
                            perm.value == GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY) {
                        permFound2 = true
                    }
                }
                assertThat("Notification permission must be present on refresh", permFound2, equalTo(true))
            }
        })
        mainSession.reload()
        mainSession.waitForPageStop()
    }

    @Test
    fun autoplayReject() {
        // The profile used in automation sets this to false, so we need to hack it back to true here.
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "media.geckoview.autoplay.request" to true))

        mainSession.loadTestPath(AUTOPLAY_PATH)

        mainSession.waitUntilCalled(object : Callbacks.PermissionDelegate {
            @AssertCalled(count = 2)
            override fun onContentPermissionRequest(session: GeckoSession, uri: String?, type: Int, callback: GeckoSession.PermissionDelegate.Callback) {
                val expectedType = if (sessionRule.currentCall.counter == 1) GeckoSession.PermissionDelegate.PERMISSION_AUTOPLAY_AUDIBLE else GeckoSession.PermissionDelegate.PERMISSION_AUTOPLAY_INAUDIBLE
                assertThat("Type should match", type, equalTo(expectedType))
                callback.reject()
            }
        })
    }

    // @Test fun persistentStorage() {
    //     mainSession.loadTestPath(HELLO_HTML_PATH)
    //     mainSession.waitForPageStop()

    //     // Persistent storage can be rejected
    //     mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
    //         @AssertCalled(count = 1)
    //         override fun onContentPermissionRequest(
    //                 session: GeckoSession, uri: String?, type: Int,
    //                 callback: GeckoSession.PermissionDelegate.Callback) {
    //             callback.reject()
    //         }
    //     })

    //     var success = mainSession.waitForJS("""window.navigator.storage.persist()""")

    //     assertThat("Request should fail",
    //             success as Boolean, equalTo(false))

    //     // Persistent storage can be granted
    //     mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
    //         // Ensure the content permission is asked first, before the Android permission.
    //         @AssertCalled(count = 1, order = [1])
    //         override fun onContentPermissionRequest(
    //                 session: GeckoSession, uri: String?, type: Int,
    //                 callback: GeckoSession.PermissionDelegate.Callback) {
    //             assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
    //             assertThat("Type should match", type,
    //                     equalTo(GeckoSession.PermissionDelegate.PERMISSION_PERSISTENT_STORAGE))
    //             callback.grant()
    //         }
    //     })

    //     success = mainSession.waitForJS("""window.navigator.storage.persist()""")

    //     assertThat("Request should succeed",
    //             success as Boolean,
    //             equalTo(true))

    //     // after permission granted further requests will always return true, regardless of response
    //     mainSession.delegateDuringNextWait(object : Callbacks.PermissionDelegate {
    //         @AssertCalled(count = 1)
    //         override fun onContentPermissionRequest(
    //                 session: GeckoSession, uri: String?, type: Int,
    //                 callback: GeckoSession.PermissionDelegate.Callback) {
    //             callback.reject()
    //         }
    //     })

    //     success = mainSession.waitForJS("""window.navigator.storage.persist()""")

    //     assertThat("Request should succeed",
    //             success as Boolean, equalTo(true))
    // }
}
