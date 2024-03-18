/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.permission

import android.net.Uri
import android.webkit.PermissionRequest
import android.webkit.PermissionRequest.RESOURCE_AUDIO_CAPTURE
import android.webkit.PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID
import android.webkit.PermissionRequest.RESOURCE_VIDEO_CAPTURE
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SystemPermissionRequestTest {

    @Test
    fun `uri is equal to native request origin`() {
        val nativeRequest: PermissionRequest = mock()
        whenever(nativeRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))
        whenever(nativeRequest.resources).thenReturn(emptyArray())
        val request = SystemPermissionRequest(nativeRequest)
        assertEquals(request.uri, "https://mozilla.org")
    }

    @Test
    fun `resources are correctly mapped to permissions`() {
        val nativeRequest: PermissionRequest = mock()
        whenever(nativeRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))
        whenever(nativeRequest.resources).thenReturn(
            arrayOf(
                RESOURCE_AUDIO_CAPTURE,
                RESOURCE_VIDEO_CAPTURE,
                RESOURCE_PROTECTED_MEDIA_ID,
            ),
        )

        val expected = listOf(
            Permission.ContentAudioCapture(RESOURCE_AUDIO_CAPTURE),
            Permission.ContentVideoCapture(RESOURCE_VIDEO_CAPTURE),
            Permission.ContentProtectedMediaId(RESOURCE_PROTECTED_MEDIA_ID),
        )
        val request = SystemPermissionRequest(nativeRequest)
        assertEquals(expected, request.permissions)
    }

    @Test
    fun `reject denies native request`() {
        val nativeRequest: PermissionRequest = mock()
        whenever(nativeRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))
        whenever(nativeRequest.resources).thenReturn(emptyArray())

        val request = SystemPermissionRequest(nativeRequest)
        request.reject()
        verify(nativeRequest).deny()
    }

    @Test
    fun `grant permission to all native request resources`() {
        val resources = arrayOf(
            RESOURCE_AUDIO_CAPTURE,
            RESOURCE_VIDEO_CAPTURE,
            RESOURCE_PROTECTED_MEDIA_ID,
        )

        val nativeRequest: PermissionRequest = mock()
        whenever(nativeRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))
        whenever(nativeRequest.resources).thenReturn(resources)

        val request = SystemPermissionRequest(nativeRequest)
        request.grant()
        verify(nativeRequest).grant(eq(resources))
    }

    @Test
    fun `grant permission to selected native request resources`() {
        val resources = arrayOf(
            RESOURCE_AUDIO_CAPTURE,
            RESOURCE_VIDEO_CAPTURE,
            RESOURCE_PROTECTED_MEDIA_ID,
        )

        val nativeRequest: PermissionRequest = mock()
        whenever(nativeRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))
        whenever(nativeRequest.resources).thenReturn(resources)

        val request = SystemPermissionRequest(nativeRequest)
        request.grant(listOf(Permission.ContentAudioCapture(RESOURCE_AUDIO_CAPTURE)))
        verify(nativeRequest).grant(eq(arrayOf(RESOURCE_AUDIO_CAPTURE)))
    }
}
