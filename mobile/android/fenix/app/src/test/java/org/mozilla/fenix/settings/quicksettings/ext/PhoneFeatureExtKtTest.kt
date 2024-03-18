/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.ext

import android.content.Context
import android.content.pm.PackageManager
import io.mockk.every
import io.mockk.mockk
import mozilla.components.concept.engine.permission.SitePermissions
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.settings.PhoneFeature

class PhoneFeatureExtKtTest {
    @Test
    fun `shouldBeVisible returns if the user made a decision about the permission`() {
        val noDecisionForPermission = mockk<SitePermissions>()
        val userAllowedPermission = mockk<SitePermissions>()
        val userBlockedPermission = mockk<SitePermissions>()
        every { noDecisionForPermission.camera } returns SitePermissions.Status.NO_DECISION
        every { userAllowedPermission.camera } returns SitePermissions.Status.ALLOWED
        every { userBlockedPermission.camera } returns SitePermissions.Status.BLOCKED

        assertFalse(PhoneFeature.CAMERA.shouldBeVisible(noDecisionForPermission, mockk()))
        assertTrue(PhoneFeature.CAMERA.shouldBeVisible(userAllowedPermission, mockk()))
        assertTrue(PhoneFeature.CAMERA.shouldBeVisible(userBlockedPermission, mockk()))
        // The site doesn't have a site permission exception
        assertFalse(PhoneFeature.CAMERA.shouldBeVisible(null, mockk()))
    }

    @Test
    fun `isUserPermissionGranted returns if user allowed or denied a permission`() {
        val noDecisionForPermission = mockk<SitePermissions>()
        val userAllowedPermission = mockk<SitePermissions>()
        val userBlockedPermission = mockk<SitePermissions>()
        every { noDecisionForPermission.camera } returns SitePermissions.Status.NO_DECISION
        every { userAllowedPermission.camera } returns SitePermissions.Status.ALLOWED
        every { userBlockedPermission.camera } returns SitePermissions.Status.BLOCKED

        assertTrue(PhoneFeature.CAMERA.isUserPermissionGranted(userAllowedPermission, mockk()))
        assertFalse(PhoneFeature.CAMERA.isUserPermissionGranted(noDecisionForPermission, mockk()))
        assertFalse(PhoneFeature.CAMERA.isUserPermissionGranted(userBlockedPermission, mockk()))
    }

    @Test
    fun `shouldBeEnabled returns if permission is granted by user and Android`() {
        val androidPermissionGrantedContext = mockk<Context>()
        val androidPermissionDeniedContext = mockk<Context>()
        val userAllowedPermission = mockk<SitePermissions>()
        val noDecisionForPermission = mockk<SitePermissions>()
        val userBlockedPermission = mockk<SitePermissions>()
        every { androidPermissionGrantedContext.checkPermission(any(), any(), any()) }
            .returns(PackageManager.PERMISSION_GRANTED)
        every { androidPermissionDeniedContext.checkPermission(any(), any(), any()) }
            .returns(PackageManager.PERMISSION_DENIED)
        every { userAllowedPermission.camera } returns SitePermissions.Status.ALLOWED
        every { noDecisionForPermission.camera } returns SitePermissions.Status.NO_DECISION
        every { userBlockedPermission.camera } returns SitePermissions.Status.BLOCKED

        // Check result for when the Android permission is granted to the app
        assertTrue(PhoneFeature.CAMERA.shouldBeEnabled(androidPermissionGrantedContext, userAllowedPermission, mockk()))
        assertFalse(PhoneFeature.CAMERA.shouldBeEnabled(androidPermissionGrantedContext, noDecisionForPermission, mockk()))
        assertFalse(PhoneFeature.CAMERA.shouldBeEnabled(androidPermissionGrantedContext, userBlockedPermission, mockk()))

        // Check result for when the Android permission is denied to the app
        assertFalse(PhoneFeature.CAMERA.shouldBeEnabled(androidPermissionDeniedContext, userAllowedPermission, mockk()))
        assertFalse(PhoneFeature.CAMERA.shouldBeEnabled(androidPermissionDeniedContext, noDecisionForPermission, mockk()))
        assertFalse(PhoneFeature.CAMERA.shouldBeEnabled(androidPermissionDeniedContext, userBlockedPermission, mockk()))
    }
}
