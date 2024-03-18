/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.permission

import android.webkit.PermissionRequest.RESOURCE_AUDIO_CAPTURE
import android.webkit.PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID
import android.webkit.PermissionRequest.RESOURCE_VIDEO_CAPTURE
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.PermissionRequest

/**
 * WebView-based implementation of [PermissionRequest].
 *
 * @property nativeRequest the underlying WebView permission request.
 */
class SystemPermissionRequest(private val nativeRequest: android.webkit.PermissionRequest) : PermissionRequest {
    override val uri: String = nativeRequest.origin.toString()
    override val id: String = java.util.UUID.randomUUID().toString()

    override val permissions = nativeRequest.resources.map { resource ->
        permissionsMap.getOrElse(resource) { Permission.Generic(resource) }
    }

    override fun grant(permissions: List<Permission>) {
        nativeRequest.grant(permissions.map { it.id }.toTypedArray())
    }

    override fun reject() {
        nativeRequest.deny()
    }

    companion object {
        val permissionsMap = mapOf(
            RESOURCE_AUDIO_CAPTURE to Permission.ContentAudioCapture(RESOURCE_AUDIO_CAPTURE),
            RESOURCE_VIDEO_CAPTURE to Permission.ContentVideoCapture(RESOURCE_VIDEO_CAPTURE),
            RESOURCE_PROTECTED_MEDIA_ID to Permission.ContentProtectedMediaId(RESOURCE_PROTECTED_MEDIA_ID),
        )
    }
}
