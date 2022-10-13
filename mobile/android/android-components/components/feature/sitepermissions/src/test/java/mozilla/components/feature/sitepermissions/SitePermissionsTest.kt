/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.AUTOPLAY_AUDIBLE
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.AUTOPLAY_INAUDIBLE
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.BLUETOOTH
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.CAMERA
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.LOCAL_STORAGE
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.LOCATION
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.MICROPHONE
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission.NOTIFICATION
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
        assertEquals(BLOCKED, sitePermissions[AUTOPLAY_AUDIBLE])
        assertEquals(ALLOWED, sitePermissions[AUTOPLAY_INAUDIBLE])

        sitePermissions = sitePermissions.copy(
            location = ALLOWED,
            notification = BLOCKED,
            microphone = NO_DECISION,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.BLOCKED,
        )

        assertEquals(BLOCKED, sitePermissions[NOTIFICATION])
        assertEquals(ALLOWED, sitePermissions[LOCATION])
        assertEquals(NO_DECISION, sitePermissions[MICROPHONE])
        assertEquals(NO_DECISION, sitePermissions[BLUETOOTH])
        assertEquals(NO_DECISION, sitePermissions[CAMERA])
        assertEquals(NO_DECISION, sitePermissions[LOCAL_STORAGE])
        assertEquals(ALLOWED, sitePermissions[AUTOPLAY_AUDIBLE])
        assertEquals(BLOCKED, sitePermissions[AUTOPLAY_INAUDIBLE])
    }

    @Test
    fun `AutoplayStatus - toStatus`() {
        var sitePermissions = SitePermissions(
            origin = "mozilla.dev",
            autoplayInaudible = AutoplayStatus.BLOCKED,
            autoplayAudible = AutoplayStatus.BLOCKED,
            savedAt = 0,
        )

        assertEquals(BLOCKED, sitePermissions.autoplayAudible.toStatus())
        assertEquals(BLOCKED, sitePermissions.autoplayInaudible.toStatus())

        sitePermissions = sitePermissions.copy(
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
        )

        assertEquals(ALLOWED, sitePermissions.autoplayAudible.toStatus())
        assertEquals(ALLOWED, sitePermissions.autoplayInaudible.toStatus())
    }

    @Test
    fun `Status to AutoplayStatus`() {
        assertEquals(AutoplayStatus.BLOCKED, BLOCKED.toAutoplayStatus())
        assertEquals(AutoplayStatus.ALLOWED, ALLOWED.toAutoplayStatus())
        assertEquals(AutoplayStatus.BLOCKED, NO_DECISION.toAutoplayStatus())
    }

    @Test
    fun `AutoplayStatus ids are aligned with Status`() {
        assertEquals(AutoplayStatus.BLOCKED.id, AutoplayStatus.BLOCKED.id)
        assertEquals(AutoplayStatus.ALLOWED, AutoplayStatus.ALLOWED)
    }
}
