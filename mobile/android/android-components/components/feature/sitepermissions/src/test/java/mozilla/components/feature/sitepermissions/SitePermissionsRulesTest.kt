/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.Permission.ContentNotification
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.Permission.Generic
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.AutoplayAction
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
class SitePermissionsRulesTest {

    private lateinit var anchorView: View
    private lateinit var rules: SitePermissionsFeature
    private lateinit var mockOnNeedToRequestPermissions: OnNeedToRequestPermissions
    private lateinit var mockStorage: SitePermissionsStorage

    @Before
    fun setup() {
        anchorView = View(testContext)
        mockOnNeedToRequestPermissions = mock()
        mockStorage = mock()

        rules = SitePermissionsFeature(
            context = testContext,
            onNeedToRequestPermissions = mockOnNeedToRequestPermissions,
            storage = mockStorage,
            fragmentManager = mock(),
            onShouldShowRequestPermissionRationale = mock(),
            store = BrowserStore(),
        )
    }

    @Test
    fun `getActionFrom must return the right action per permission`() {
        val rules = SitePermissionsRules(
            camera = ASK_TO_ALLOW,
            location = BLOCKED,
            notification = ASK_TO_ALLOW,
            microphone = BLOCKED,
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.ALLOWED,
            persistentStorage = BLOCKED,
            crossOriginStorageAccess = ALLOWED,
            mediaKeySystemAccess = ASK_TO_ALLOW,
        )

        val mockRequest: PermissionRequest = mock()

        doReturn(listOf(ContentGeoLocation())).`when`(mockRequest).permissions
        var action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.location)

        doReturn(listOf(ContentNotification())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.notification)

        doReturn(listOf(ContentAudioCapture())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.microphone)

        doReturn(listOf(ContentVideoCapture())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.camera)

        doReturn(listOf(Permission.ContentAutoPlayAudible())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.autoplayAudible.toAction())

        doReturn(listOf(Permission.ContentAutoPlayInaudible())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.autoplayInaudible.toAction())

        doReturn(listOf(Generic("", ""))).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.camera)

        doReturn(listOf(Permission.ContentPersistentStorage())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.persistentStorage)

        doReturn(listOf(Permission.ContentCrossOriginStorageAccess())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.crossOriginStorageAccess)

        doReturn(listOf(Permission.ContentMediaKeySystemAccess())).`when`(mockRequest).permissions
        action = rules.getActionFrom(mockRequest)
        assertEquals(action, rules.mediaKeySystemAccess)
    }

    @Test
    fun `getActionFrom must return the right action for a Camera + Microphone permission`() {
        var rules = SitePermissionsRules(
            camera = ASK_TO_ALLOW,
            location = BLOCKED,
            crossOriginStorageAccess = ALLOWED,
            persistentStorage = BLOCKED,
            notification = ASK_TO_ALLOW,
            microphone = BLOCKED,
            autoplayInaudible = AutoplayAction.ALLOWED,
            autoplayAudible = AutoplayAction.BLOCKED,
            mediaKeySystemAccess = ASK_TO_ALLOW,
        )

        val mockRequest: PermissionRequest = mock()
        doReturn(true).`when`(mockRequest).containsVideoAndAudioSources()

        var action = rules.getActionFrom(mockRequest)
        assertEquals(action, BLOCKED)

        rules = SitePermissionsRules(
            camera = ASK_TO_ALLOW,
            location = BLOCKED,
            crossOriginStorageAccess = ALLOWED,
            notification = ASK_TO_ALLOW,
            microphone = ASK_TO_ALLOW,
            autoplayInaudible = AutoplayAction.ALLOWED,
            autoplayAudible = AutoplayAction.BLOCKED,
            persistentStorage = BLOCKED,
            mediaKeySystemAccess = ASK_TO_ALLOW,
        )

        action = rules.getActionFrom(mockRequest)
        assertEquals(action, ASK_TO_ALLOW)
    }

    @Test
    fun `toSitePermissions - converts a SitePermissionsRules to SitePermissions`() {
        val expectedSitePermission = SitePermissions(
            origin = "origin",
            camera = Status.NO_DECISION,
            location = Status.BLOCKED,
            localStorage = Status.BLOCKED,
            crossOriginStorageAccess = Status.ALLOWED,
            notification = Status.NO_DECISION,
            microphone = Status.BLOCKED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            autoplayAudible = AutoplayStatus.BLOCKED,
            mediaKeySystemAccess = Status.BLOCKED,
            savedAt = 1L,
        )

        val rules = SitePermissionsRules(
            camera = ASK_TO_ALLOW,
            location = BLOCKED,
            notification = ASK_TO_ALLOW,
            microphone = BLOCKED,
            autoplayInaudible = AutoplayAction.ALLOWED,
            autoplayAudible = AutoplayAction.BLOCKED,
            persistentStorage = BLOCKED,
            crossOriginStorageAccess = ALLOWED,
            mediaKeySystemAccess = BLOCKED,
        )

        val convertedSitePermissions = rules.toSitePermissions(origin = "origin", savedAt = 1L)

        assertEquals(expectedSitePermission.origin, convertedSitePermissions.origin)
        assertEquals(expectedSitePermission.camera, convertedSitePermissions.camera)
        assertEquals(expectedSitePermission.location, convertedSitePermissions.location)
        assertEquals(expectedSitePermission.notification, convertedSitePermissions.notification)
        assertEquals(expectedSitePermission.microphone, convertedSitePermissions.microphone)
        assertEquals(expectedSitePermission.autoplayInaudible, convertedSitePermissions.autoplayInaudible)
        assertEquals(expectedSitePermission.autoplayAudible, convertedSitePermissions.autoplayAudible)
        assertEquals(expectedSitePermission.localStorage, convertedSitePermissions.localStorage)
        assertEquals(expectedSitePermission.crossOriginStorageAccess, convertedSitePermissions.crossOriginStorageAccess)
        assertEquals(expectedSitePermission.mediaKeySystemAccess, convertedSitePermissions.mediaKeySystemAccess)
        assertEquals(expectedSitePermission.savedAt, convertedSitePermissions.savedAt)
    }

    @Test
    fun `AutoplayAction - toAutoplayStatus`() {
        assertEquals(AutoplayStatus.ALLOWED, AutoplayAction.ALLOWED.toAutoplayStatus())
        assertEquals(AutoplayStatus.BLOCKED, AutoplayAction.BLOCKED.toAutoplayStatus())
    }
}
