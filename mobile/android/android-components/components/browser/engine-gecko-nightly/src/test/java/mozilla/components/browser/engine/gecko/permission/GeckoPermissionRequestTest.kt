/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.permission

import android.Manifest
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.ContentAutoplayMedia
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.GeckoSession
import org.robolectric.RobolectricTestRunner

import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_AUTOPLAY_MEDIA
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION

@RunWith(RobolectricTestRunner::class)
class GeckoPermissionRequestTest {

    @Test
    fun `create content permission request`() {
        val callback: GeckoSession.PermissionDelegate.Callback = mock()
        val uri = "https://mozilla.org"

        var request = GeckoPermissionRequest.Content(uri, PERMISSION_AUTOPLAY_MEDIA, callback)
        assertEquals(uri, request.uri)
        assertEquals(listOf(ContentAutoplayMedia()), request.permissions)

        request = GeckoPermissionRequest.Content(uri, PERMISSION_DESKTOP_NOTIFICATION, callback)
        assertEquals(uri, request.uri)
        assertEquals(listOf(Permission.ContentNotification()), request.permissions)

        request = GeckoPermissionRequest.Content(uri, PERMISSION_GEOLOCATION, callback)
        assertEquals(uri, request.uri)
        assertEquals(listOf(Permission.ContentGeoLocation()), request.permissions)

        request = GeckoPermissionRequest.Content(uri, 1234, callback)
        assertEquals(uri, request.uri)
        assertEquals(listOf(Permission.Generic("1234", "Gecko permission type = 1234")), request.permissions)
    }

    @Test
    fun `grant content permission request`() {
        val callback: GeckoSession.PermissionDelegate.Callback = mock()
        val uri = "https://mozilla.org"

        var request = GeckoPermissionRequest.Content(uri, PERMISSION_AUTOPLAY_MEDIA, callback)
        request.grant()
        verify(callback).grant()
    }

    @Test
    fun `reject content permission request`() {
        val callback: GeckoSession.PermissionDelegate.Callback = mock()
        val uri = "https://mozilla.org"

        var request = GeckoPermissionRequest.Content(uri, PERMISSION_AUTOPLAY_MEDIA, callback)
        request.reject()
        verify(callback).reject()
    }

    @Test
    fun `create app permission request`() {
        val callback: GeckoSession.PermissionDelegate.Callback = mock()
        val permissions = listOf(
                Manifest.permission.ACCESS_COARSE_LOCATION,
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.CAMERA,
                Manifest.permission.RECORD_AUDIO,
                "unknown app permission")

        val mappedPermissions = listOf(
                Permission.AppLocationCoarse(Manifest.permission.ACCESS_COARSE_LOCATION),
                Permission.AppLocationFine(Manifest.permission.ACCESS_FINE_LOCATION),
                Permission.AppCamera(Manifest.permission.CAMERA),
                Permission.AppAudio(Manifest.permission.RECORD_AUDIO),
                Permission.Generic("unknown app permission")
        )

        var request = GeckoPermissionRequest.App(permissions, callback)
        assertEquals(mappedPermissions, request.permissions)
    }

    @Test
    fun `grant app permission request`() {
        val callback: GeckoSession.PermissionDelegate.Callback = mock()
        val uri = "https://mozilla.org"

        var request = GeckoPermissionRequest.App(listOf(Manifest.permission.CAMERA), callback)
        request.grant()
        verify(callback).grant()
    }

    @Test
    fun `reject app permission request`() {
        val callback: GeckoSession.PermissionDelegate.Callback = mock()
        val uri = "https://mozilla.org"

        var request = GeckoPermissionRequest.App(listOf(Manifest.permission.CAMERA), callback)
        request.reject()
        verify(callback).reject()
    }

    @Test
    fun `create media permission request`() {
        val callback: GeckoSession.PermissionDelegate.MediaCallback = mock()
        val uri = "https://mozilla.org"

        val audioMicrophone = getMediaSource("audioMicrophone", "audioMicrophone", "microphone", "audioinput")
        val audioCapture = getMediaSource("audioCapture", "audioCapture", "audioCapture", "audioinput")
        val audioOther = getMediaSource("audioOther", "audioOther", "other", "audioinput")

        val videoCamera = getMediaSource("videoCamera", "videoCamera", "camera", "videoinput")
        val videoBrowser = getMediaSource("videoBrowser", "videoBrowser", "browser", "videoinput")
        val videoApplication = getMediaSource("videoApplication", "videoApplication", "application", "videoinput")
        val videoScreen = getMediaSource("videoScreen", "videoScreen", "screen", "videoinput")
        val videoWindow = getMediaSource("videoWindow", "videoWindow", "window", "videoinput")
        val videoOther = getMediaSource("videoOther", "videoOther", "other", "videoinput")

        val audioSources = listOf(audioCapture, audioMicrophone, audioOther)
        val videoSources = listOf(videoApplication, videoBrowser, videoCamera, videoOther, videoScreen, videoWindow)

        val mappedPermissions = listOf(
                Permission.ContentVideoCamera("videoCamera", "videoCamera"),
                Permission.ContentVideoBrowser("videoBrowser", "videoBrowser"),
                Permission.ContentVideoApplication("videoApplication", "videoApplication"),
                Permission.ContentVideoScreen("videoScreen", "videoScreen"),
                Permission.ContentVideoWindow("videoWindow", "videoWindow"),
                Permission.ContentVideoOther("videoOther", "videoOther"),
                Permission.ContentAudioMicrophone("audioMicrophone", "audioMicrophone"),
                Permission.ContentAudioCapture("audioCapture", "audioCapture"),
                Permission.ContentAudioOther("audioOther", "audioOther")
        )

        var request = GeckoPermissionRequest.Media(uri, videoSources, audioSources, callback)
        assertEquals(uri, request.uri)
        assertEquals(mappedPermissions.size, request.permissions.size)
        assertTrue(request.permissions.containsAll(mappedPermissions))
    }

    @Test
    fun `grant media permission request`() {
        val callback: GeckoSession.PermissionDelegate.MediaCallback = mock()
        val uri = "https://mozilla.org"

        val audioMicrophone = getMediaSource("audioMicrophone", "audioMicrophone", "microphone", "audioinput")
        val videoCamera = getMediaSource("videoCamera", "videoCamera", "camera", "videoinput")

        val audioSources = listOf(audioMicrophone)
        val videoSources = listOf(videoCamera)

        var request = GeckoPermissionRequest.Media(uri, videoSources, audioSources, callback)
        request.grant(request.permissions)
        verify(callback).grant(videoCamera, audioMicrophone)
    }

    @Test
    fun `reject media permission request`() {
        val callback: GeckoSession.PermissionDelegate.MediaCallback = mock()
        val uri = "https://mozilla.org"

        val audioMicrophone = getMediaSource("audioMicrophone", "audioMicrophone", "microphone", "audioinput")
        val videoCamera = getMediaSource("videoCamera", "videoCamera", "camera", "videoinput")

        val audioSources = listOf(audioMicrophone)
        val videoSources = listOf(videoCamera)

        var request = GeckoPermissionRequest.Media(uri, videoSources, audioSources, callback)
        request.reject()
        verify(callback).reject()
    }

    private fun getMediaSource(id: String, name: String, source: String, type: String): GeckoSession.PermissionDelegate.MediaSource {
        val bundle = GeckoBundle()
        bundle.putString("id", id)
        bundle.putString("name", name)
        bundle.putString("mediaSource", source)
        bundle.putString("type", type)

        val constructor = GeckoSession.PermissionDelegate.MediaSource::class.java.getDeclaredConstructor(GeckoBundle::class.java)
        constructor.isAccessible = true
        return constructor.newInstance(bundle)
    }
}
