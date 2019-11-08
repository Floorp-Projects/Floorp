/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.feature.sitepermissions.SitePermissions.Status.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.NOTIFICATION
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.LOCAL_STORAGE
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.LOCATION
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.MICROPHONE
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.BLUETOOTH
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.CAMERA
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SitePermissionsTest {
    @Test
    fun `Tests get() against direct property access`() {
        var sitePermissions = SitePermissions(origin = "mozilla.dev", savedAt = 0)

        assertEquals(NO_DECISION, sitePermissions[NOTIFICATION])
        assertEquals(NO_DECISION, sitePermissions[LOCATION])
        assertEquals(NO_DECISION, sitePermissions[LOCAL_STORAGE])
        assertEquals(NO_DECISION, sitePermissions[MICROPHONE])
        assertEquals(NO_DECISION, sitePermissions[BLUETOOTH])
        assertEquals(NO_DECISION, sitePermissions[CAMERA])

        sitePermissions = sitePermissions.copy(
            location = ALLOWED,
            notification = BLOCKED,
            microphone = NO_DECISION
        )

        assertEquals(BLOCKED, sitePermissions[NOTIFICATION])
        assertEquals(ALLOWED, sitePermissions[LOCATION])
        assertEquals(NO_DECISION, sitePermissions[MICROPHONE])
        assertEquals(NO_DECISION, sitePermissions[BLUETOOTH])
        assertEquals(NO_DECISION, sitePermissions[CAMERA])
        assertEquals(NO_DECISION, sitePermissions[LOCAL_STORAGE])
    }
}
