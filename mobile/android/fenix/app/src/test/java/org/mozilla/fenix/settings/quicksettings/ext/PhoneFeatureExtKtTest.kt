/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.ext

import android.content.Context
import android.content.pm.PackageManager
import assertk.assertAll
import assertk.assertThat
import assertk.assertions.isFalse
import assertk.assertions.isTrue
import io.mockk.every
import io.mockk.mockk
import mozilla.components.feature.sitepermissions.SitePermissions
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

        assertAll {
            assertThat(PhoneFeature.CAMERA.shouldBeVisible(noDecisionForPermission, mockk())).isFalse()
            assertThat(PhoneFeature.CAMERA.shouldBeVisible(userAllowedPermission, mockk())).isTrue()
            assertThat(PhoneFeature.CAMERA.shouldBeVisible(userBlockedPermission, mockk())).isTrue()
        }
    }

    @Test
    fun `isUserPermissionGranted returns if user allowed or denied a permission`() {
        val noDecisionForPermission = mockk<SitePermissions>()
        val userAllowedPermission = mockk<SitePermissions>()
        val userBlockedPermission = mockk<SitePermissions>()
        every { noDecisionForPermission.camera } returns SitePermissions.Status.NO_DECISION
        every { userAllowedPermission.camera } returns SitePermissions.Status.ALLOWED
        every { userBlockedPermission.camera } returns SitePermissions.Status.BLOCKED

        assertAll {
            assertThat(PhoneFeature.CAMERA.isUserPermissionGranted(userAllowedPermission, mockk())).isTrue()
            assertThat(PhoneFeature.CAMERA.isUserPermissionGranted(noDecisionForPermission, mockk())).isFalse()
            assertThat(PhoneFeature.CAMERA.isUserPermissionGranted(userBlockedPermission, mockk())).isFalse()
        }
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

        assertAll {
            // Check result for when the Android permission is granted to the app
            assertThat(PhoneFeature.CAMERA.shouldBeEnabled(
                androidPermissionGrantedContext, userAllowedPermission, mockk())).isTrue()
            assertThat(PhoneFeature.CAMERA.shouldBeEnabled(
                androidPermissionGrantedContext, noDecisionForPermission, mockk())).isFalse()
            assertThat(PhoneFeature.CAMERA.shouldBeEnabled(
                androidPermissionGrantedContext, userBlockedPermission, mockk())).isFalse()

            // Check result for when the Android permission is denied to the app
            assertThat(PhoneFeature.CAMERA.shouldBeEnabled(
                androidPermissionDeniedContext, userAllowedPermission, mockk())).isFalse()
            assertThat(PhoneFeature.CAMERA.shouldBeEnabled(
                androidPermissionDeniedContext, noDecisionForPermission, mockk())).isFalse()
            assertThat(PhoneFeature.CAMERA.shouldBeEnabled(
                androidPermissionDeniedContext, userBlockedPermission, mockk())).isFalse()
        }
    }
}
