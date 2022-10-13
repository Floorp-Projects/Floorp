/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions.db

import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import org.junit.Assert.assertEquals
import org.junit.Test

class SitePermissionEntityTest {

    @Test
    fun `convert from db entity to domain class`() {
        val dbEntity = SitePermissionsEntity(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = BLOCKED,
            location = BLOCKED,
            notification = NO_DECISION,
            microphone = NO_DECISION,
            camera = NO_DECISION,
            bluetooth = ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            autoplayAudible = AutoplayStatus.BLOCKED,
            mediaKeySystemAccess = NO_DECISION,
            savedAt = 0,
        )

        val domainClass = dbEntity.toSitePermission()

        with(dbEntity) {
            assertEquals(origin, domainClass.origin)
            assertEquals(localStorage, domainClass.localStorage)
            assertEquals(crossOriginStorageAccess, domainClass.crossOriginStorageAccess)
            assertEquals(location, domainClass.location)
            assertEquals(notification, domainClass.notification)
            assertEquals(microphone, domainClass.microphone)
            assertEquals(camera, domainClass.camera)
            assertEquals(bluetooth, domainClass.bluetooth)
            assertEquals(autoplayAudible, domainClass.autoplayAudible)
            assertEquals(autoplayInaudible, domainClass.autoplayInaudible)
            assertEquals(mediaKeySystemAccess, domainClass.mediaKeySystemAccess)
            assertEquals(savedAt, domainClass.savedAt)
        }
    }

    @Test
    fun `convert from domain class to db entity`() {
        val domainClass = SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = BLOCKED,
            location = BLOCKED,
            notification = NO_DECISION,
            microphone = NO_DECISION,
            camera = NO_DECISION,
            bluetooth = ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            autoplayAudible = AutoplayStatus.BLOCKED,
            mediaKeySystemAccess = NO_DECISION,
            savedAt = 0,
        )

        val dbEntity = domainClass.toSitePermissionsEntity()

        with(dbEntity) {
            assertEquals(origin, domainClass.origin)
            assertEquals(localStorage, domainClass.localStorage)
            assertEquals(crossOriginStorageAccess, domainClass.crossOriginStorageAccess)
            assertEquals(location, domainClass.location)
            assertEquals(notification, domainClass.notification)
            assertEquals(microphone, domainClass.microphone)
            assertEquals(camera, domainClass.camera)
            assertEquals(bluetooth, domainClass.bluetooth)
            assertEquals(autoplayAudible, domainClass.autoplayAudible)
            assertEquals(autoplayInaudible, domainClass.autoplayInaudible)
            assertEquals(mediaKeySystemAccess, domainClass.mediaKeySystemAccess)
            assertEquals(savedAt, domainClass.savedAt)
        }
    }
}
