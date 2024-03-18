/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.permission

import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class PermissionRequestTest {

    @Test
    fun `grantIf applies predicate to grant (but not reject) permission requests`() {
        var request = MockPermissionRequest(listOf(Permission.ContentProtectedMediaId()))
        request.grantIf { it is Permission.ContentAudioCapture }
        assertFalse(request.granted)
        assertFalse(request.rejected)

        request.grantIf { it is Permission.ContentProtectedMediaId }
        assertTrue(request.granted)
        assertFalse(request.rejected)

        request = MockPermissionRequest(listOf(Permission.Generic("test"), Permission.ContentProtectedMediaId()))
        request.grantIf { it is Permission.Generic && it.id == "nomatch" }
        assertFalse(request.granted)
        assertFalse(request.rejected)

        request.grantIf { it is Permission.Generic && it.id == "test" }
        assertTrue(request.granted)
        assertFalse(request.rejected)
    }

    @Test
    fun `permission types are not equal if fields differ`() {
        assertNotEquals(Permission.Generic("id"), Permission.Generic("id2"))
        assertNotEquals(Permission.Generic("id"), Permission.Generic("id", "desc"))

        assertNotEquals(Permission.ContentAudioCapture(), Permission.ContentAudioCapture("id"))
        assertNotEquals(Permission.ContentAudioCapture("id"), Permission.ContentAudioCapture("id", "desc"))
        assertNotEquals(Permission.ContentAudioMicrophone(), Permission.ContentAudioMicrophone("id"))
        assertNotEquals(Permission.ContentAudioMicrophone("id"), Permission.ContentAudioMicrophone("id", "desc"))
        assertNotEquals(Permission.ContentAudioOther(), Permission.ContentAudioOther("id"))
        assertNotEquals(Permission.ContentAudioOther("id"), Permission.ContentAudioOther("id", "desc"))

        assertNotEquals(Permission.ContentProtectedMediaId(), Permission.ContentProtectedMediaId("id"))
        assertNotEquals(Permission.ContentProtectedMediaId("id"), Permission.ContentProtectedMediaId("id", "desc"))
        assertNotEquals(Permission.ContentGeoLocation(), Permission.ContentGeoLocation("id"))
        assertNotEquals(Permission.ContentGeoLocation("id"), Permission.ContentGeoLocation("id", "desc"))
        assertNotEquals(Permission.ContentNotification(), Permission.ContentNotification("id"))
        assertNotEquals(Permission.ContentNotification("id"), Permission.ContentNotification("id", "desc"))

        assertNotEquals(Permission.ContentVideoCamera(), Permission.ContentVideoCamera("id"))
        assertNotEquals(Permission.ContentVideoCamera("id"), Permission.ContentVideoCamera("id", "desc"))
        assertNotEquals(Permission.ContentVideoCapture(), Permission.ContentVideoCapture("id"))
        assertNotEquals(Permission.ContentVideoCapture("id"), Permission.ContentVideoCapture("id", "desc"))
        assertNotEquals(Permission.ContentVideoScreen(), Permission.ContentVideoScreen("id"))
        assertNotEquals(Permission.ContentVideoScreen("id"), Permission.ContentVideoScreen("id", "desc"))
        assertNotEquals(Permission.ContentVideoOther(), Permission.ContentVideoOther("id"))
        assertNotEquals(Permission.ContentVideoOther("id"), Permission.ContentVideoOther("id", "desc"))

        assertNotEquals(Permission.AppAudio(), Permission.AppAudio("id"))
        assertNotEquals(Permission.AppAudio("id"), Permission.AppAudio("id", "desc"))
        assertNotEquals(Permission.AppCamera(), Permission.AppCamera("id"))
        assertNotEquals(Permission.AppCamera("id"), Permission.AppCamera("id", "desc"))
        assertNotEquals(Permission.AppLocationCoarse(), Permission.AppLocationCoarse("id"))
        assertNotEquals(Permission.AppLocationCoarse("id"), Permission.AppLocationCoarse("id", "desc"))
        assertNotEquals(Permission.AppLocationFine(), Permission.AppLocationFine("id"))
        assertNotEquals(Permission.AppLocationFine("id"), Permission.AppLocationFine("id", "desc"))
    }

    private class MockPermissionRequest(
        override val permissions: List<Permission>,
        override val uri: String = "",
        override val id: String = "",
    ) : PermissionRequest {
        var granted = false
        var rejected = false

        override fun grant(permissions: List<Permission>) {
            granted = true
        }

        override fun reject() {
            rejected = true
        }
    }
}
