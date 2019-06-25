/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.Manifest
import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentAudioMicrophone
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.Permission.ContentNotification
import mozilla.components.concept.engine.permission.Permission.ContentVideoCamera
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.Permission.Generic
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.feature.sitepermissions.SitePermissions.Status.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.security.InvalidParameterException
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class SitePermissionsFeatureTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var sitePermissionFeature: SitePermissionsFeature
    private lateinit var mockOnNeedToRequestPermissions: OnNeedToRequestPermissions
    private lateinit var mockStorage: SitePermissionsStorage
    private lateinit var mockFragmentManager: FragmentManager

    @Before
    fun setup() {
        val engine = mock<Engine>()
        mockSessionManager = Mockito.spy(SessionManager(engine))
        mockOnNeedToRequestPermissions = mock()
        mockStorage = mock()
        mockFragmentManager = mockFragmentManager()

        sitePermissionFeature = SitePermissionsFeature(
            context = testContext,
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = mockOnNeedToRequestPermissions,
            storage = mockStorage,
            fragmentManager = mockFragmentManager
        )
    }

    @Test
    fun `a new onAppPermissionRequested will call onNeedToRequestPermissions`() {

        val session = getSelectedSession()

        var wasCalled = false

        sitePermissionFeature = SitePermissionsFeature(
            context = testContext,
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = {
                wasCalled = true
            },
            fragmentManager = mockFragmentManager
        )

        sitePermissionFeature.start()

        val mockPermissionRequest: PermissionRequest = mock()
        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        assertTrue(wasCalled)
    }

    @Test
    fun `requesting an invalid content permission request will reject the permission request`() {
        val session = getSelectedSession()

        sitePermissionFeature = SitePermissionsFeature(
            context = testContext,
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = {
            },
            fragmentManager = mockFragmentManager
        )

        sitePermissionFeature.start()

        val mockPermissionRequest: PermissionRequest = mock()

        doReturn(listOf(Generic("", ""))).`when`(mockPermissionRequest).permissions

        session.contentPermissionRequest = Consumable.from(mockPermissionRequest)

        verify(mockPermissionRequest).reject()
    }

    @Test
    fun `after calling stop the feature will not be notified by new incoming permissionRequests`() {

        val session = getSelectedSession()

        var wasCalled = false

        sitePermissionFeature = SitePermissionsFeature(
            context = testContext,
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = {
                wasCalled = true
            },
            fragmentManager = mockFragmentManager
        )

        sitePermissionFeature.start()

        val mockPermissionRequest: PermissionRequest = mock()
        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        assertTrue(wasCalled)

        wasCalled = false

        sitePermissionFeature.stop()
        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        assertFalse(wasCalled)
    }

    @Test
    fun `providing a custom session id to the feature will notified on new incoming permissionRequests`() {
        val customTabSession = Session("customTabSessionId")
        val selectedSession = Session("")

        mockSessionManager.add(selectedSession)
        mockSessionManager.select(selectedSession)
        mockSessionManager.add(customTabSession)

        var wasCalled = false

        sitePermissionFeature = SitePermissionsFeature(
            context = testContext,
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = {
                wasCalled = true
            },
            sessionId = customTabSession.id,
            fragmentManager = mockFragmentManager
        )

        sitePermissionFeature.start()

        val mockPermissionRequest: PermissionRequest = mock()
        customTabSession.appPermissionRequest = Consumable.from(mockPermissionRequest)

        assertTrue(wasCalled)
    }

    @Test
    fun `granting a content permission must call grant and consume contentPermissionRequest`() {
        val permissions = listOf(
            ContentVideoCapture("", "back camera"),
            ContentVideoCamera("", "front camera"),
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone()
        )

        permissions.forEach { permission ->
            val session = getSelectedSession()
            var grantWasCalled = false

            val permissionRequest: PermissionRequest = object : PermissionRequest {
                override val uri: String?
                    get() = "http://www.mozilla.org"
                override val permissions: List<Permission>
                    get() = listOf(permission)

                override fun grant(permissions: List<Permission>) {
                    grantWasCalled = true
                }

                override fun reject() = Unit
            }

            mockStorage = mock()
            session.contentPermissionRequest = Consumable.from(permissionRequest)

            grantPermission(Manifest.permission.RECORD_AUDIO)
            runBlocking {
                val prompt = sitePermissionFeature.onContentPermissionRequested(session, permissionRequest)
                assertNotNull(prompt)
                sitePermissionFeature.onPositiveButtonPress(session.id, false)
                assertTrue(grantWasCalled)
                assertTrue(session.contentPermissionRequest.isConsumed())
            }
        }
    }

    @Test
    fun `onContentPermissionRequested with rules must behave according to the rules object`() {
        val permissions = listOf(
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone()
        )

        val rules = SitePermissionsRules(
            location = SitePermissionsRules.Action.BLOCKED,
            camera = SitePermissionsRules.Action.ASK_TO_ALLOW,
            notification = SitePermissionsRules.Action.ASK_TO_ALLOW,
            microphone = SitePermissionsRules.Action.BLOCKED
        )

        sitePermissionFeature.sitePermissionsRules = rules
        grantPermission(Manifest.permission.RECORD_AUDIO)
        permissions.forEach { permission ->
            val session = getSelectedSession()
            var grantWasCalled = false
            var rejectWasCalled = false

            val permissionRequest: PermissionRequest = object : PermissionRequest {
                override val uri: String?
                    get() = "http://www.mozilla.org"
                override val permissions: List<Permission>
                    get() = listOf(permission)

                override fun grant(permissions: List<Permission>) {
                    grantWasCalled = true
                }

                override fun reject() {
                    rejectWasCalled = true
                }
            }

            mockStorage = mock()
            session.contentPermissionRequest = Consumable.from(permissionRequest)

            runBlocking {
                val prompt = sitePermissionFeature.onContentPermissionRequested(session, permissionRequest)

                when (permission) {
                    is ContentGeoLocation, is ContentAudioCapture, is ContentAudioMicrophone -> {
                        assertTrue(rejectWasCalled)
                        assertFalse(grantWasCalled)
                        assertNull(prompt)
                        assertTrue(session.contentPermissionRequest.isConsumed())
                    }
                    is ContentVideoCamera, is ContentVideoCapture, is ContentNotification -> {
                        assertFalse(rejectWasCalled)
                        assertFalse(grantWasCalled)
                        assertNotNull(prompt)
                    }

                    else -> throw InvalidParameterException()
                }
            }
        }
    }

    @Test
    fun `storing a new SitePermissions must call save on the store`() {
        val sitePermissionsList = listOf(ContentGeoLocation())
        val request: PermissionRequest = mock()

        runBlocking {
            sitePermissionFeature.coroutineScopeInitializer = {
                this
            }
            sitePermissionFeature.storeSitePermissions(mock(), request, sitePermissionsList, ALLOWED)
        }
        verify(mockStorage).findSitePermissionsBy(anyString())
        verify(mockStorage).save(any())
    }

    @Test
    fun `storing a SitePermissions with a private session must NOT call save on the store`() {
        val sitePermissionsList = listOf(ContentGeoLocation())
        val request: PermissionRequest = mock()
        val session: Session = mock()

        doReturn(true).`when`(session).private

        runBlocking {
            sitePermissionFeature.coroutineScopeInitializer = {
                this
            }
            sitePermissionFeature.storeSitePermissions(session, request, sitePermissionsList, ALLOWED)
        }

        verify(mockStorage, times(0)).save(any())
    }

    @Test
    fun `storing an already existing SitePermissions must call update on the store`() {
        val sitePermissionsList = listOf(
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone(),
            Generic(id = null)
        )

        var exceptionThrown = false

        sitePermissionsList.forEachIndexed { index, permission ->
            val mockRequest: PermissionRequest = mock()
            val sitePermissionFromStorage: SitePermissions = mock()
            val permissions = listOf(permission)
            doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString())
            doReturn(permissions).`when`(mockRequest).permissions

            val feature = SitePermissionsFeature(
                context = testContext,
                sessionManager = mockSessionManager,
                fragmentManager = mockFragmentManager,
                onNeedToRequestPermissions = mockOnNeedToRequestPermissions,
                storage = mockStorage
            )

            try {
                runBlocking {
                    feature.coroutineScopeInitializer = { this }
                    feature.storeSitePermissions(mock(), mockRequest, permissions, ALLOWED)
                }
                verify(mockStorage, times(index + 1)).findSitePermissionsBy(anyString())
                verify(mockStorage, times(index + 1)).update(any())
            } catch (e: InvalidParameterException) {
                exceptionThrown = true
            }
        }
        assertTrue(exceptionThrown)
    }

    @Test
    fun `requesting a content permissions with an already stored allowed permissions will auto granted it and not show a prompt`() {
        val request: PermissionRequest = mock()
        val mockSession: Session = mock()
        val mockConsumable: Consumable<PermissionRequest> = mock()
        val sitePermissionFromStorage: SitePermissions = mock()
        val permissionList = listOf(ContentGeoLocation())

        doReturn(permissionList).`when`(request).permissions
        doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString())
        doReturn(ALLOWED).`when`(sitePermissionFromStorage).location
        doReturn(mockConsumable).`when`(mockSession).contentPermissionRequest
        doReturn(true).`when`(mockConsumable).consume { true }

        runBlocking {
            val prompt = sitePermissionFeature.onContentPermissionRequested(mockSession, request)
            verify(mockStorage).findSitePermissionsBy(anyString())
            verify(request).grant(permissionList)
            assertNull(prompt)
        }
    }

    @Test
    fun `requesting a content permissions with an already stored blocked permission will auto block it and not show a prompt`() {
        val request: PermissionRequest = mock()
        val mockSession: Session = mock()
        val mockConsumable: Consumable<PermissionRequest> = mock()
        val sitePermissionFromStorage: SitePermissions = mock()
        val permissionList = listOf(ContentGeoLocation())

        doReturn(permissionList).`when`(request).permissions
        doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString())
        doReturn(BLOCKED).`when`(sitePermissionFromStorage).location
        doReturn(mockConsumable).`when`(mockSession).contentPermissionRequest
        doReturn(true).`when`(mockConsumable).consume { true }

        runBlocking {
            val prompt = sitePermissionFeature.onContentPermissionRequested(mockSession, request)
            verify(mockStorage).findSitePermissionsBy(anyString())
            verify(request).reject()
            assertNull(prompt)
        }
    }

    @Test
    fun `requesting a content permissions with already permissions and doNotAskAgain equals false on the store, will show a prompt`() {
        val request: PermissionRequest = mock()
        val sitePermissionFromStorage: SitePermissions = mock()
        val permissionList = listOf(ContentGeoLocation())

        doReturn(permissionList).`when`(request).permissions
        doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString())
        doReturn(NO_DECISION).`when`(sitePermissionFromStorage).location

        runBlocking {
            val prompt = sitePermissionFeature.onContentPermissionRequested(getSelectedSession(), request)
            verify(mockStorage).findSitePermissionsBy(anyString())
            verify(request, times(0)).grant(permissionList)
            assertNotNull(prompt)
        }
    }

    @Test
    fun `is SitePermission granted in the storage`() {
        val sitePermissionsList = listOf(
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone(),
            ContentVideoCamera(),
            ContentVideoCapture()
        )

        sitePermissionsList.forEach { permission ->
            val request: PermissionRequest = mock()
            val sitePermissionFromStorage: SitePermissions = mock()

            doReturn(listOf(permission)).`when`(request).permissions
            doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString())
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).location
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).notification
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).camera
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).microphone

            val isAllowed = sitePermissionFromStorage.isGranted(request)
            assertTrue(isAllowed)
        }
    }

    @Test
    fun `is SitePermission blocked in the storage`() {
        val sitePermissionsList = listOf(
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone(),
            ContentVideoCamera(),
            ContentVideoCapture(),
            Generic(id = null)
        )

        var exceptionThrown = false
        sitePermissionsList.forEach { permission ->
            val request: PermissionRequest = mock()
            val sitePermissionFromStorage: SitePermissions = mock()

            doReturn(listOf(permission)).`when`(request).permissions
            doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString())
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).location
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).notification
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).camera
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).microphone

            try {
                val isAllowed = sitePermissionFromStorage.isGranted(request)
                assertFalse(isAllowed)
            } catch (e: InvalidParameterException) {
                exceptionThrown = true
            }
        }
        assertTrue(exceptionThrown)
    }

    @Test
    fun `rejecting a content permission must call reject and consume contentPermissionRequest`() {
        val permissions = listOf(
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone()
        )

        permissions.forEach { permission ->
            val session = getSelectedSession()
            var rejectWasCalled = false

            val permissionRequest: PermissionRequest = object : PermissionRequest {
                override val uri: String?
                    get() = "http://www.mozilla.org"
                override val permissions: List<Permission>
                    get() = listOf(permission)

                override fun reject() {
                    rejectWasCalled = true
                }

                override fun grant(permissions: List<Permission>) = Unit
            }
            grantPermission(Manifest.permission.RECORD_AUDIO)

            session.contentPermissionRequest = Consumable.from(permissionRequest)

            runBlocking {
                val prompt = sitePermissionFeature.onContentPermissionRequested(session, permissionRequest)

                sitePermissionFeature.onNegativeButtonPress(session.id, false)

                assertNotNull(prompt)
                assertTrue(rejectWasCalled)
                assertTrue(session.contentPermissionRequest.isConsumed())
            }
        }
    }

    @Test
    fun `rejecting a camera content permission must call reject and consume contentPermissionRequest`() {

        val permissions = listOf(
            ContentVideoCapture("", "back camera"),
            ContentVideoCamera("", "front camera")
        )

        permissions.forEachIndexed { index, _ ->

            val session = getSelectedSession()
            var rejectWasCalled = false

            val permissionRequest: PermissionRequest = object : PermissionRequest {
                override val uri: String?
                    get() = "http://www.mozilla.org"

                override val permissions: List<Permission>
                    get() = if (index > 0) permissions.reversed() else permissions

                override fun reject() {
                    rejectWasCalled = true
                }

                override fun grant(permissions: List<Permission>) = Unit
            }

            session.contentPermissionRequest = Consumable.from(permissionRequest)

            runBlocking {
                val prompt = sitePermissionFeature.onContentPermissionRequested(session, permissionRequest)

                sitePermissionFeature.onNegativeButtonPress(session.id, false)

                assertNotNull(prompt)
                assertTrue(rejectWasCalled)
                assertTrue(session.contentPermissionRequest.isConsumed())
            }
        }
    }

    @Test
    fun `granting a camera and microphone permission must call grant and consume contentPermissionRequest`() {
        val session = getSelectedSession()
        var grantWasCalled = false
        val permissionRequest: PermissionRequest = object : PermissionRequest {
            override val uri: String?
                get() = "http://www.mozilla.org"

            override val permissions: List<Permission>
                get() = listOf(
                    ContentVideoCapture("", "back camera"),
                    ContentVideoCamera("", "front camera"),
                    ContentAudioMicrophone()
                )

            override fun grant(permissions: List<Permission>) {
                grantWasCalled = true
            }

            override fun containsVideoAndAudioSources() = true

            override fun reject() = Unit
        }

        session.contentPermissionRequest = Consumable.from(permissionRequest)

        runBlocking {
            val prompt = sitePermissionFeature.onContentPermissionRequested(session, permissionRequest)

            sitePermissionFeature.onPositiveButtonPress(session.id, false)

            assertNotNull(prompt)
            assertTrue(grantWasCalled)
            assertTrue(session.contentPermissionRequest.isConsumed())
        }
    }

    @Test
    fun `rejecting a camera and microphone permission must call reject and consume contentPermissionRequest`() {
        val session = getSelectedSession()
        var rejectWasCalled = false
        val permissionRequest: PermissionRequest = object : PermissionRequest {
            override val uri: String?
                get() = "http://www.mozilla.org"

            override val permissions: List<Permission>
                get() = listOf(
                    ContentVideoCapture("", "back camera"),
                    ContentVideoCamera("", "front camera"),
                    ContentAudioMicrophone()
                )

            override fun reject() {
                rejectWasCalled = true
            }

            override fun containsVideoAndAudioSources() = true

            override fun grant(permissions: List<Permission>) = Unit
        }

        session.contentPermissionRequest = Consumable.from(permissionRequest)

        runBlocking {
            val prompt = sitePermissionFeature.onContentPermissionRequested(session, permissionRequest)
            sitePermissionFeature.onNegativeButtonPress(session.id, false)

            assertNotNull(prompt)
            assertTrue(rejectWasCalled)
            assertTrue(session.contentPermissionRequest.isConsumed())
        }
    }

    @Test
    fun `calling onPermissionsResult with all permissions granted will call grant on the permissionsRequest and consume it`() {
        val session = getSelectedSession()
        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        sitePermissionFeature.onPermissionsResult(emptyArray(), intArrayOf(PERMISSION_GRANTED))

        verify(mockPermissionRequest).grant(emptyList())
        assertTrue(session.appPermissionRequest.isConsumed())
    }

    @Test
    fun `calling onPermissionsResult on a feature session with a sessionId will call grant on the permissionsRequest and consume it`() {
        val session = Session("", id = "sessionIdCustomTabs")
        sitePermissionFeature = SitePermissionsFeature(
            context = testContext,
            sessionId = "sessionIdCustomTabs",
            sessionManager = mockSessionManager,
            onNeedToRequestPermissions = mockOnNeedToRequestPermissions,
            storage = mockStorage,
            fragmentManager = mockFragmentManager
        )

        mockSessionManager.add(session)

        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        sitePermissionFeature.onPermissionsResult(emptyArray(), intArrayOf(PERMISSION_GRANTED))

        verify(mockPermissionRequest).grant(emptyList())
        assertTrue(session.appPermissionRequest.isConsumed())
    }

    @Test
    fun `calling onPermissionsResult with NOT all permissions granted will call reject on the permissionsRequest and consume it`() {
        val session = getSelectedSession()
        val mockPermissionRequest: PermissionRequest = mock()

        sitePermissionFeature.start()

        session.appPermissionRequest = Consumable.from(mockPermissionRequest)

        sitePermissionFeature.onPermissionsResult(emptyArray(), intArrayOf(PERMISSION_DENIED))

        verify(mockPermissionRequest).reject()
        assertTrue(session.appPermissionRequest.isConsumed())
    }

    @Test
    fun `feature will re-attach to already existing fragment`() {
        val mockPermissionRequest: PermissionRequest = mock()
        val session = getSelectedSession().apply {
            appPermissionRequest = Consumable.from(mockPermissionRequest)
        }

        val fragment: SitePermissionsDialogFragment = mock()
        doReturn(session.id).`when`(fragment).sessionId

        doReturn(fragment).`when`(mockFragmentManager).findFragmentByTag(any())

        sitePermissionFeature.start()
        verify(fragment).feature = sitePermissionFeature
    }

    @Test
    fun `already existing fragment will be removed if session has none permissions request set anymore`() {
        val session = getSelectedSession()

        val fragment: SitePermissionsDialogFragment = mock()

        doReturn(session.id).`when`(fragment).sessionId

        val transaction: FragmentTransaction = mock()

        doReturn(fragment).`when`(mockFragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(mockFragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        sitePermissionFeature.start()
        verify(mockFragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `already existing fragment will be removed if session does not exist anymore`() {
        val fragment: SitePermissionsDialogFragment = mock()
        doReturn(UUID.randomUUID().toString()).`when`(fragment).sessionId

        val transaction: FragmentTransaction = mock()

        doReturn(fragment).`when`(mockFragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(mockFragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        sitePermissionFeature.start()
        verify(mockFragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }

    private fun getSelectedSession(): Session {
        val session = Session("")
        mockSessionManager.add(session)
        mockSessionManager.select(session)
        return session
    }
}
