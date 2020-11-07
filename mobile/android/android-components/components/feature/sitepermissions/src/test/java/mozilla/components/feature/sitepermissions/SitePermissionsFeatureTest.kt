/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runBlockingTest
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.AppAudio
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentAudioMicrophone
import mozilla.components.concept.engine.permission.Permission.ContentAutoPlayInaudible
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
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.anyString
import org.mockito.Mockito.atLeastOnce
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.security.InvalidParameterException
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class SitePermissionsFeatureTest {
    private lateinit var sitePermissionFeature: SitePermissionsFeature
    private lateinit var mockOnNeedToRequestPermissions: OnNeedToRequestPermissions
    private lateinit var mockStorage: SitePermissionsStorage
    private lateinit var mockFragmentManager: FragmentManager
    private lateinit var mockStore: BrowserStore
    private lateinit var mockContentState: ContentState
    private lateinit var mockPermissionRequest: PermissionRequest
    private lateinit var mockAppPermissionRequest: PermissionRequest
    private lateinit var mockSitePermissionRules: SitePermissionsRules
    private lateinit var selectedTab: TabSessionState

    private val testCoroutineDispatcher = TestCoroutineDispatcher()
    private val testScope = CoroutineScope(testCoroutineDispatcher)

    companion object {
        const val SESSION_ID = "testSessionId"
        const val URL = "http://www.example.com"
        const val PERMISSION_ID = "PERMISSION_ID"
    }

    @Before
    fun setup() {
        Dispatchers.setMain(testCoroutineDispatcher)
        mockOnNeedToRequestPermissions = mock()
        mockStorage = mock()
        mockFragmentManager = mockFragmentManager()
        mockContentState = mock()
        mockPermissionRequest = mock()
        mockAppPermissionRequest = mock()
        mockSitePermissionRules = mock()

        selectedTab = mozilla.components.browser.state.state.createTab(
            url = "https://www.mozilla.org",
            id = SESSION_ID
        )
        mockStore = spy(BrowserStore(initialState = BrowserState(tabs = listOf(selectedTab), selectedTabId = selectedTab.id)))
        sitePermissionFeature = spy(
            SitePermissionsFeature(
                context = testContext,
                sitePermissionsRules = mockSitePermissionRules,
                onNeedToRequestPermissions = mockOnNeedToRequestPermissions,
                storage = mockStorage,
                fragmentManager = mockFragmentManager,
                onShouldShowRequestPermissionRationale = { false },
                store = mockStore,
                sessionId = SESSION_ID
            )
        )
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testCoroutineDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `GIVEN a sessionId WHEN calling consumePermissionRequest THEN dispatch ConsumePermissionsRequest action with given id`() {
        // when
        sitePermissionFeature.consumePermissionRequest(mockPermissionRequest, "sessionIdTest")

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumePermissionsRequest
                ("sessionIdTest", mockPermissionRequest)
        )
    }

    @Test
    fun `GIVEN no sessionId WHEN calling consumePermissionRequest THEN dispatch ConsumePermissionsRequest action with selected tab`() {
        // when
        sitePermissionFeature.consumePermissionRequest(mockPermissionRequest)

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumePermissionsRequest
                (selectedTab.id, mockPermissionRequest)
        )
    }

    @Test
    fun `GIVEN a sessionId WHEN calling consumeAppPermissionRequest THEN dispatch ConsumeAppPermissionsRequest action with given id`() {
        // when
        sitePermissionFeature.consumeAppPermissionRequest(mockAppPermissionRequest, "sessionIdTest")

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumeAppPermissionsRequest
                ("sessionIdTest", mockAppPermissionRequest)
        )
    }

    @Test
    fun `GIVEN no sessionId WHEN calling consumeAppPermissionRequest THEN dispatch ConsumeAppPermissionsRequest action with selected tab`() {
        // when
        sitePermissionFeature.consumeAppPermissionRequest(mockAppPermissionRequest)

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumeAppPermissionsRequest
                (selectedTab.id, mockAppPermissionRequest)
        )
    }

    @Test
    fun `GIVEN an appPermissionRequest with all granted permissions WHEN onPermissionsResult() THEN grant() and consumeAppPermissionRequest() are called`() {
        // given
        doReturn(mockAppPermissionRequest).`when`(sitePermissionFeature)
            .findRequestedAppPermission(any())

        // when
        sitePermissionFeature.onPermissionsResult(arrayOf("permission"), arrayOf(PERMISSION_GRANTED).toIntArray())

        // then
        verify(mockAppPermissionRequest).grant()
        verify(sitePermissionFeature).consumeAppPermissionRequest(mockAppPermissionRequest)
    }

    @Test
    fun `GIVEN an appPermissionRequest with blocked permissions and !onShouldShowRequestPermissionRationale WHEN onPermissionsResult() THEN reject(), storeSitePermissions, consume are called`() {
        // given
        doReturn(mockAppPermissionRequest).`when`(sitePermissionFeature)
            .findRequestedAppPermission(any())
        doNothing().`when`(sitePermissionFeature)
            .storeSitePermissions(any(), any(), any(), any())

        // when
        sitePermissionFeature.onPermissionsResult(
            arrayOf("permission1", "permission2"),
            arrayOf(PERMISSION_DENIED, PERMISSION_DENIED).toIntArray()
        )

        // then
        verify(mockAppPermissionRequest).reject()
        verify(sitePermissionFeature, times(2))
            .storeSitePermissions(selectedTab.content, mockAppPermissionRequest, BLOCKED)
        verify(sitePermissionFeature).consumeAppPermissionRequest(mockAppPermissionRequest)
    }

    @Test
    fun `GIVEN shouldStore true WHEN onContentPermissionGranted() THEN storeSitePermissions() called`() {
        // given
        doNothing().`when`(sitePermissionFeature)
            .storeSitePermissions(any(), any(), any(), any())

        // when
        sitePermissionFeature.onContentPermissionGranted(mockPermissionRequest, true)

        // then
        verify(sitePermissionFeature)
            .storeSitePermissions(selectedTab.content, mockPermissionRequest, ALLOWED)
    }

    @Test
    fun `GIVEN shouldStore false WHEN onContentPermissionGranted() THEN storeSitePermissions() MUST NOT BE called`() {
        // given
        doNothing().`when`(sitePermissionFeature)
                .storeSitePermissions(any(), any(), any(), any())

        // when
        sitePermissionFeature.onContentPermissionGranted(mockPermissionRequest, false)

        // then
        verify(sitePermissionFeature, never())
                .storeSitePermissions(selectedTab.content, mockPermissionRequest, ALLOWED)
    }

    @Test
    fun `GIVEN permissionRequest WHEN onPositiveButtonPress() THEN consumePermissionRequest, onContentPermissionGranted are called`() {
        // given
        doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
        doNothing().`when`(sitePermissionFeature)
            .onContentPermissionGranted(mockPermissionRequest, true)
        doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(
            anyString())

        // when
        sitePermissionFeature.onPositiveButtonPress(PERMISSION_ID, SESSION_ID, true)

        // then
        verify(sitePermissionFeature)
            .consumePermissionRequest(mockPermissionRequest, SESSION_ID)
        verify(sitePermissionFeature)
            .onContentPermissionGranted(mockPermissionRequest, true)
    }

    @Test
    fun `GIVEN permissionRequest WHEN onNegativeButtonPress() THEN consumePermissionRequest, onContentPermissionDeny are called`() {
        // given
        doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
        doNothing().`when`(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, true)
        doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(
            anyString())

        // when
        sitePermissionFeature.onNegativeButtonPress(PERMISSION_ID, SESSION_ID, true)

        // then
        verify(sitePermissionFeature)
            .consumePermissionRequest(mockPermissionRequest, SESSION_ID)
        verify(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, true)
    }

    @Test
    fun `GIVEN permissionRequest WHEN onDismiss() THEN consumePermissionRequest, onContentPermissionDeny are called`() {
        // given
        doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
        doNothing().`when`(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, false)
        doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(
            anyString())

        // when
        sitePermissionFeature.onDismiss(PERMISSION_ID, SESSION_ID)

        // then
        verify(sitePermissionFeature)
            .consumePermissionRequest(mockPermissionRequest, SESSION_ID)
        verify(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, false)
    }

    @Test
    fun `GIVEN a new permissionRequest WHEN storeSitePermissions() THEN save(permissionRequest) is called`() {
        // given
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        doReturn(null).`when`(mockStorage).findSitePermissionsBy(ArgumentMatchers.anyString())
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(Permission.AppAudio(id = "permission")))
        }
        doReturn(sitePermissions).`when`(sitePermissionFeature)
            .updateSitePermissionsStatus(any(), any(), any())
        whenever(mockContentState.url).thenReturn(URL)

        // when
        sitePermissionFeature.storeSitePermissions(
            mockContentState,
            mockPermissionRequest,
            ALLOWED,
            testScope
        )

        // then
        verify(mockStorage).save(sitePermissions)
    }

    @Test
    fun `GIVEN an already saved permissionRequest WHEN storeSitePermissions() THEN update(permissionRequest) is called`() {
        // given
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        doReturn(sitePermissions).`when`(mockStorage)
            .findSitePermissionsBy(ArgumentMatchers.anyString())
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(AppAudio(id = "permission")))
        }
        doReturn(sitePermissions).`when`(sitePermissionFeature)
            .updateSitePermissionsStatus(any(), any(), any())
        whenever(mockContentState.url).thenReturn(URL)

        // when
        sitePermissionFeature.storeSitePermissions(
            mockContentState,
            mockPermissionRequest,
            ALLOWED,
            testScope
        )

        // then
        verify(mockStorage).update(sitePermissions)
    }

    @Test
    fun `GIVEN a permissionRequest WITH a private tab WHEN storeSitePermissions() THEN save or update MUST NOT BE called`() {
        // then
        sitePermissionFeature.storeSitePermissions(
            selectedTab.content.copy(private = true),
            mockPermissionRequest,
            ALLOWED,
            testScope
        )

        // when
        verify(mockStorage, never()).save(any())
        verify(mockStorage, never()).update(any())
    }

    @Test
    fun `GIVEN permissionRequest WHEN onContentPermissionDeny THEN reject(), storeSitePermissions are called`() {
        // given
        doNothing().`when`(mockPermissionRequest).reject()
        doNothing().`when`(sitePermissionFeature).storeSitePermissions(any(), any(), any(), any())

        // then
        sitePermissionFeature.onContentPermissionDeny(mockPermissionRequest, true)

        // when
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).storeSitePermissions(
            selectedTab.content,
            mockPermissionRequest,
            BLOCKED
        )
    }

    @Test
    fun `GIVEN a media permissionRequest without all media permissions granted WHEN onContentPermissionRequested() THEN reject, consumePermissionRequest are called `() {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentVideoCamera(id = "permission")))
        }
        doNothing().`when`(mockPermissionRequest).reject()

        // when
        runBlockingTest {
            sitePermissionFeature.onContentPermissionRequested(mockPermissionRequest, URL)
        }

        // then
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
    }

    @Test
    fun `GIVEN location permissionRequest and shouldApplyRules is true WHEN onContentPermissionRequested() THEN handleRuledFlow is called`() {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        val mockedSitePermissionsDialogFragment = SitePermissionsDialogFragment()
        doReturn(sitePermissions).`when`(mockStorage).findSitePermissionsBy(URL)
        doReturn(true).`when`(sitePermissionFeature).shouldApplyRules(any())
        doReturn(mockedSitePermissionsDialogFragment).`when`(sitePermissionFeature)
            .handleRuledFlow(mockPermissionRequest, URL)

        // when
        runBlockingTest {
            sitePermissionFeature.onContentPermissionRequested(
                mockPermissionRequest,
                URL,
                testScope
            )
        }

        // then
        verify(mockStorage).findSitePermissionsBy(URL)
        verify(sitePermissionFeature).handleRuledFlow(mockPermissionRequest, URL)
    }

    @Test
    fun `GIVEN location permissionRequest and shouldApplyRules is false WHEN onContentPermissionRequested() THEN handleNoRuledFlow is called`() {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        val sitePermissionsDialogFragment = SitePermissionsDialogFragment()
        doReturn(sitePermissions).`when`(mockStorage).findSitePermissionsBy(URL)
        doReturn(false).`when`(sitePermissionFeature).shouldApplyRules(any())
        doReturn(sitePermissionsDialogFragment).`when`(sitePermissionFeature)
            .handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)

        // when
        runBlockingTest {
            sitePermissionFeature.onContentPermissionRequested(
                mockPermissionRequest,
                URL,
                testScope
            )
        }

        // then
        verify(mockStorage).findSitePermissionsBy(URL)
        verify(sitePermissionFeature).handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)
    }

    @Test
    fun `GIVEN shouldShowPrompt true WHEN handleNoRuledFlow THEN createPrompt is called`() {
        // given
        val sitePermissionsDialogFragment = SitePermissionsDialogFragment()
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        doReturn(true).`when`(sitePermissionFeature)
            .shouldShowPrompt(mockPermissionRequest, sitePermissions)
        doReturn(sitePermissionsDialogFragment).`when`(sitePermissionFeature)
            .createPrompt(any(), any())

        // when
        sitePermissionFeature.handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)

        // then
        verify(sitePermissionFeature).createPrompt(mockPermissionRequest, URL)
    }

    @Test
    fun `GIVEN shouldShowPrompt false and permissionFromStorage not granted WHEN handleNoRuledFlow THEN reject, consumePermissionRequest are called `() {
        // given
        doReturn(false).`when`(sitePermissionFeature).shouldShowPrompt(mockPermissionRequest, null)

        // when
        sitePermissionFeature.handleNoRuledFlow(null, mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
    }

    @Test
    fun `GIVEN shouldShowPrompt false and permissionFromStorage granted WHEN handleNoRuledFlow THEN grant, consumePermissionRequest are called `() {
        // given
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0, location = ALLOWED)
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        doReturn(false).`when`(sitePermissionFeature)
            .shouldShowPrompt(mockPermissionRequest, sitePermissions)

        // when
        sitePermissionFeature.handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest, atLeastOnce()).grant()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
    }

    @Test
    fun `GIVEN permissionRequest with isForAutoplay true and sitePermissionsRules null WHEN handleRuledFlow THEN reject, consumePermissionRequest are called`() {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAutoPlayInaudible(id = "permission")))
        }

        val sitePermissionFeature = spy(
            SitePermissionsFeature(
                context = testContext,
                sitePermissionsRules = null,
                onNeedToRequestPermissions = mockOnNeedToRequestPermissions,
                storage = mockStorage,
                fragmentManager = mockFragmentManager,
                onShouldShowRequestPermissionRationale = { false },
                store = mockStore,
                sessionId = SESSION_ID
            )
        )

        // when
        sitePermissionFeature.handleRuledFlow(mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
    }

    @Test
    fun `GIVEN permissionRequest with isForAutoplay false and ALLOWED WHEN handleRuledFlow THEN grant, consumePermissionRequest are called`() {
        // given
        val mockPermissionsRules: SitePermissionsRules = mock()
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        doReturn(mockPermissionsRules).`when`(sitePermissionFeature).sitePermissionsRules
        doReturn(SitePermissionsRules.Action.ALLOWED).`when`(mockPermissionsRules)
            .getActionFrom(mockPermissionRequest)

        // when
        sitePermissionFeature.handleRuledFlow(mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest).grant()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
    }

    @Test
    fun `GIVEN permissionRequest with isForAutoplay false and BLOCKED WHEN handleRuledFlow THEN reject, consumePermissionRequest are called`() {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        doReturn(mockSitePermissionRules).`when`(sitePermissionFeature).sitePermissionsRules
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules)
            .getActionFrom(mockPermissionRequest)

        // when
        sitePermissionFeature.handleRuledFlow(mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
    }

    @Test
    fun `GIVEN permissionRequest with isForAutoplay false and ASK_TO_ALLOW WHEN handleRuledFlow THEN createPrompt is called`() {
        // given
        val sitePermissionsDialogFragment = SitePermissionsDialogFragment()
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        doReturn(mockSitePermissionRules).`when`(sitePermissionFeature).sitePermissionsRules
        doReturn(SitePermissionsRules.Action.ASK_TO_ALLOW).`when`(mockSitePermissionRules)
            .getActionFrom(mockPermissionRequest)
        doReturn(sitePermissionsDialogFragment).`when`(sitePermissionFeature)
            .createPrompt(mockPermissionRequest, URL)

        // when
        sitePermissionFeature.handleRuledFlow(mockPermissionRequest, URL)

        // then
        verify(sitePermissionFeature).createPrompt(mockPermissionRequest, URL)
    }

    @Test
    fun `GIVEN permissionRequest and containsVideoAndAudioSources false WHEN createPrompt THEN handlingSingleContentPermissions is called`() {
        // given
        val sitePermissionsDialogFragment = SitePermissionsDialogFragment()
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        doReturn(sitePermissionsDialogFragment).`when`(sitePermissionFeature)
            .handlingSingleContentPermissions(any(), any(), any())

        // when
        sitePermissionFeature.createPrompt(mockPermissionRequest, URL)

        // then
        verify(sitePermissionFeature).handlingSingleContentPermissions(
            mockPermissionRequest, ContentGeoLocation(id = "permission"), URL
        )
    }

    @Test
    fun `GIVEN permissionRequest and containsVideoAndAudioSources true WHEN createPrompt THEN createSinglePermissionPrompt is called`() {
        // given
        val permissionRequest: PermissionRequest = object : PermissionRequest {
            override val uri: String?
                get() = "http://www.mozilla.org"
            override val id: String
                get() = PERMISSION_ID

            override val permissions: List<Permission>
                get() = listOf(
                    ContentVideoCapture("", "back camera"),
                    ContentVideoCamera("", "front camera"),
                    ContentAudioMicrophone()
                )

            override fun grant(permissions: List<Permission>) {
            }

            override fun containsVideoAndAudioSources() = true

            override fun reject() = Unit
        }
        val sitePermissionsDialogFragment = SitePermissionsDialogFragment()
        doReturn(sitePermissionsDialogFragment).`when`(sitePermissionFeature)
            .createSinglePermissionPrompt(
                any(),
                ArgumentMatchers.anyString(),
                any(),
                ArgumentMatchers.anyInt(),
                ArgumentMatchers.anyInt(),
                ArgumentMatchers.anyBoolean(),
                ArgumentMatchers.anyBoolean(),
                ArgumentMatchers.anyBoolean()
            )

        // when
        sitePermissionFeature.createPrompt(permissionRequest, URL)

        // then
        verify(sitePermissionFeature).createSinglePermissionPrompt(
            any(),
            ArgumentMatchers.anyString(),
            any(),
            ArgumentMatchers.anyInt(),
            ArgumentMatchers.anyInt(),
            ArgumentMatchers.anyBoolean(),
            ArgumentMatchers.anyBoolean(),
            ArgumentMatchers.anyBoolean()
        )
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
    fun `feature will re-attach to already existing fragment`() {
        doReturn(false).`when`(sitePermissionFeature).noPermissionRequests(any())

        val fragment: SitePermissionsDialogFragment = mock()
        doReturn(selectedTab.id).`when`(fragment).sessionId
        doReturn(fragment).`when`(mockFragmentManager).findFragmentByTag(any())

        sitePermissionFeature.start()
        verify(fragment).feature = sitePermissionFeature
    }

    @Test
    fun `already existing fragment will be removed if session has none permissions request set anymore`() {
        // given
        val session = selectedTab
        val fragment: SitePermissionsDialogFragment = mock()
        doReturn(session.id).`when`(fragment).sessionId
        val transaction: FragmentTransaction = mock()
        doReturn(fragment).`when`(mockFragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(mockFragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)
        doReturn(mockContentState).`when`(sitePermissionFeature)
            .getCurrentContentState()
        doNothing().`when`(sitePermissionFeature).setupPermissionRequestsCollector()
        doNothing().`when`(sitePermissionFeature).setupAppPermissionRequestsCollector()

        // when
        sitePermissionFeature.start()

        // then
        verify(mockFragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `already existing fragment will be removed if session does not exist anymore`() {
        val fragment: SitePermissionsDialogFragment = mock()
        doReturn(UUID.randomUUID().toString()).`when`(fragment).sessionId
        doReturn(mockContentState).`when`(sitePermissionFeature)
            .getCurrentContentState()
        doNothing().`when`(sitePermissionFeature).setupPermissionRequestsCollector()
        doNothing().`when`(sitePermissionFeature).setupAppPermissionRequestsCollector()

        val transaction: FragmentTransaction = mock()

        doReturn(fragment).`when`(mockFragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(mockFragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        sitePermissionFeature.start()
        verify(mockFragmentManager).beginTransaction()
        verify(transaction).remove(fragment)
    }

    @Test
    fun `getInitialSitePermissions - WHEN sitePermissionsRules is present the function MUST use the sitePermissionsRules values to create a SitePermissions object`() {

        val rules = SitePermissionsRules(
                location = SitePermissionsRules.Action.BLOCKED,
                camera = SitePermissionsRules.Action.ASK_TO_ALLOW,
                notification = SitePermissionsRules.Action.ASK_TO_ALLOW,
                microphone = SitePermissionsRules.Action.BLOCKED,
                autoplayAudible = SitePermissionsRules.Action.BLOCKED,
                autoplayInaudible = SitePermissionsRules.Action.ALLOWED,
                persistentStorage = SitePermissionsRules.Action.BLOCKED
        )

        sitePermissionFeature.sitePermissionsRules = rules

        val sitePermissions = sitePermissionFeature.getInitialSitePermissions(URL)

        assertEquals(URL, sitePermissions.origin)
        assertEquals(BLOCKED, sitePermissions.location)
        assertEquals(NO_DECISION, sitePermissions.camera)
        assertEquals(NO_DECISION, sitePermissions.notification)
        assertEquals(BLOCKED, sitePermissions.microphone)
        assertEquals(BLOCKED, sitePermissions.autoplayAudible)
        assertEquals(ALLOWED, sitePermissions.autoplayInaudible)
        assertEquals(BLOCKED, sitePermissions.localStorage)
    }

    @Test
    fun `any media request must be rejected WHEN system permissions are not granted first`() {
        val permissions = listOf(
                ContentVideoCapture("", "back camera"),
                ContentVideoCamera("", "front camera"),
                ContentAudioCapture(),
                ContentAudioMicrophone()
        )

        permissions.forEach { permission ->
            var grantWasCalled = false

            val permissionRequest: PermissionRequest = object : PermissionRequest {
                override val uri: String?
                    get() = "http://www.mozilla.org"
                override val id: String
                    get() = PERMISSION_ID
                override val permissions: List<Permission>
                    get() = listOf(permission)

                override fun grant(permissions: List<Permission>) {
                    grantWasCalled = true
                }

                override fun reject() = Unit
            }

            mockStorage = mock()

            runBlocking {
                val prompt = sitePermissionFeature
                    .onContentPermissionRequested(permissionRequest, URL)
                assertNull(prompt)
                assertFalse(grantWasCalled)
            }
        }
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}
