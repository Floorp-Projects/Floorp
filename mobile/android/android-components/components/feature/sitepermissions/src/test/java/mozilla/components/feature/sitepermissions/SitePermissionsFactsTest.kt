/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import mozilla.components.concept.engine.permission.Permission.AppCamera
import mozilla.components.concept.engine.permission.Permission.AppLocationFine
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentCrossOriginStorageAccess
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.Permission.Generic
import mozilla.components.support.base.Component.FEATURE_SITEPERMISSIONS
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import org.junit.Assert.assertEquals
import org.junit.Test

class SitePermissionsFactsTest {

    @Test
    fun `GIVEN a fact for a prompt shown for one permission WHEN it is emitted THEN it is properly configured`() {
        CollectionProcessor.withFactCollection { facts ->
            emitPermissionDialogDisplayed(Generic())

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.DISPLAY, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(Generic().id, facts[0].value)
        }
    }

    @Test
    fun `GIVEN a fact for a prompt shown for multiple permissions WHEN it is emitted THEN it is properly configured`() {
        CollectionProcessor.withFactCollection { facts ->
            emitPermissionsDialogDisplayed(listOf(AppCamera(), ContentCrossOriginStorageAccess()))

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.DISPLAY, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(listOf(AppCamera(), ContentCrossOriginStorageAccess()).joinToString { it.id!! }, facts[0].value)
        }
    }

    @Test
    fun `GIVEN a fact for a permission prompt being allowed WHEN it is emitted THEN it is properly configured`() {
        CollectionProcessor.withFactCollection { facts ->
            emitPermissionAllowed(ContentAudioCapture())

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CONFIRM, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(ContentAudioCapture().id, facts[0].value)
        }
    }

    @Test
    fun `GIVEN a fact for a multiple permission prompt being allowed WHEN it is emitted THEN it is properly configured`() {
        CollectionProcessor.withFactCollection { facts ->
            emitPermissionsAllowed(listOf(ContentAudioCapture(), ContentVideoCapture()))

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CONFIRM, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(listOf(ContentAudioCapture(), ContentVideoCapture()).joinToString { it.id!! }, facts[0].value)
        }
    }

    @Test
    fun `GIVEN a fact for a permission prompt being blocked WHEN it is emitted THEN it is properly configured`() {
        CollectionProcessor.withFactCollection { facts ->
            emitPermissionDenied(AppLocationFine())

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CANCEL, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(AppLocationFine().id, facts[0].value)
        }
    }

    @Test
    fun `GIVEN a fact for a multiple permission prompt being blocked WHEN it is emitted THEN it is properly configured`() {
        CollectionProcessor.withFactCollection { facts ->
            emitPermissionsDenied(listOf(ContentAudioCapture(), ContentVideoCapture()))

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CANCEL, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(listOf(ContentAudioCapture(), ContentVideoCapture()).joinToString { it.id!! }, facts[0].value)
        }
    }
}
