/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SitePermissionsFeatureTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var sitePermissionFeature: SitePermissionsFeature
    private lateinit var mockOnNeedToRequestPermissions: OnNeedToRequestPermissions

    @Before
    fun setup() {
        val engine = Mockito.mock(Engine::class.java)

        mockSessionManager = Mockito.spy(SessionManager(engine))
        mockOnNeedToRequestPermissions = mock()

        sitePermissionFeature = SitePermissionsFeature(
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = mockOnNeedToRequestPermissions
        )
    }

    @Test
    fun `a new onAppPermissionRequested will call mockOnNeedToRequestPermissions`() {

        val session = getSelectedSession()

        var wasCalled = false

        sitePermissionFeature = SitePermissionsFeature(
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = {
                wasCalled = true
            })

        sitePermissionFeature.start()

        val mockPermissionRequest: PermissionRequest = mock()
        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        assertTrue(wasCalled)
    }

    @Test
    fun `granting a content permission must call grant and consume contentPermissionRequest`() {
        val session = getSelectedSession()

        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.contentPermissionRequest = Consumable.from(mockPermissionRequest)

        sitePermissionFeature.onContentPermissionGranted(session.id, session.url, emptyList())

        verify(mockPermissionRequest).grant(emptyList())
        assertTrue(session.contentPermissionRequest.isConsumed())
    }

    @Test
    fun `granting a content permission with an unknown session will no consume pending contentPermissionRequest`() {
        val session = getSelectedSession()
        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.contentPermissionRequest = Consumable.from(mockPermissionRequest)

        assertFalse(session.contentPermissionRequest.isConsumed())

        sitePermissionFeature.onContentPermissionGranted("unknown_session", session.url, emptyList())

        assertFalse(session.contentPermissionRequest.isConsumed())
    }

    @Test
    fun `rejecting a content permission must call reject and consume contentPermissionRequest`() {
        val session = getSelectedSession()

        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.contentPermissionRequest = Consumable.from(mockPermissionRequest)

        sitePermissionFeature.onContentPermissionDeny(session.id, session.url)

        verify(mockPermissionRequest).reject()
        assertTrue(session.contentPermissionRequest.isConsumed())
    }

    @Test
    fun `rejecting a content permission with an unknown session will no consume pending contentPermissionRequest`() {
        val session = getSelectedSession()
        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.contentPermissionRequest = Consumable.from(mockPermissionRequest)

        assertFalse(session.contentPermissionRequest.isConsumed())

        sitePermissionFeature.onContentPermissionDeny("unknown_session", session.url)

        assertFalse(session.contentPermissionRequest.isConsumed())
    }

    @Test
    fun `calling onPermissionsResult with all permissions granted will call grant on the permissionsRequest and consume it`() {
        val session = getSelectedSession()
        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        sitePermissionFeature.onPermissionsResult(intArrayOf(PERMISSION_GRANTED))

        verify(mockPermissionRequest).grant(emptyList())
        assertTrue(session.appPermissionRequest.isConsumed())
    }

    @Test
    fun `calling onPermissionsResult with NOT all permissions granted will call reject on the permissionsRequest and consume it`() {
        val session = getSelectedSession()
        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        sitePermissionFeature.onPermissionsResult(intArrayOf(PERMISSION_DENIED))

        verify(mockPermissionRequest).reject()
        assertTrue(session.appPermissionRequest.isConsumed())
    }

    private fun getSelectedSession(): Session {
        val session = Session("")
        mockSessionManager.add(session)
        mockSessionManager.select(session)
        return session
    }
}