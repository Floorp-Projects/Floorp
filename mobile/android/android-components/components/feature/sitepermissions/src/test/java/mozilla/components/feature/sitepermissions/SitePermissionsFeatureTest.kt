/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.content.pm.PackageManager.PERMISSION_DENIED
import android.content.pm.PackageManager.PERMISSION_GRANTED
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayAudibleBlockingAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayAudibleChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayInAudibleBlockingAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.AutoPlayInAudibleChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.CameraChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.LocationChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.MediaKeySystemAccesChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.MicrophoneChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.NotificationChangedAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction.PersistentStorageChangedAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.Permission.AppAudio
import mozilla.components.concept.engine.permission.Permission.ContentAudioCapture
import mozilla.components.concept.engine.permission.Permission.ContentAudioMicrophone
import mozilla.components.concept.engine.permission.Permission.ContentAutoPlayAudible
import mozilla.components.concept.engine.permission.Permission.ContentAutoPlayInaudible
import mozilla.components.concept.engine.permission.Permission.ContentCrossOriginStorageAccess
import mozilla.components.concept.engine.permission.Permission.ContentGeoLocation
import mozilla.components.concept.engine.permission.Permission.ContentMediaKeySystemAccess
import mozilla.components.concept.engine.permission.Permission.ContentNotification
import mozilla.components.concept.engine.permission.Permission.ContentPersistentStorage
import mozilla.components.concept.engine.permission.Permission.ContentVideoCamera
import mozilla.components.concept.engine.permission.Permission.ContentVideoCapture
import mozilla.components.concept.engine.permission.Permission.Generic
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.feature.tabs.TabsUseCases.SelectOrAddUseCase
import mozilla.components.support.base.Component.FEATURE_SITEPERMISSIONS
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.ktx.kotlin.stripDefaultPort
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.anyBoolean
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

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    companion object {
        const val SESSION_ID = "testSessionId"
        const val URL = "http://www.example.com"
        const val PERMISSION_ID = "PERMISSION_ID"
    }

    @Before
    fun setup() {
        mockOnNeedToRequestPermissions = mock()
        mockStorage = mock()
        mockFragmentManager = mockFragmentManager()
        mockContentState = mock()
        mockPermissionRequest = mock()
        doReturn(true).`when`(mockPermissionRequest).containsVideoAndAudioSources()
        mockAppPermissionRequest = mock()
        mockSitePermissionRules = mock()

        selectedTab = mozilla.components.browser.state.state.createTab(
            url = "https://www.mozilla.org",
            id = SESSION_ID,
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
                sessionId = SESSION_ID,
            ),
        )
    }

    @Test
    fun `GIVEN a tab load THEN stale permission indicators should be clear up and temporary permissions`() {
        sitePermissionFeature.start()

        verify(sitePermissionFeature).setupLoadingCollector()

        // when
        mockStore.dispatch(ContentAction.UpdateLoadingStateAction(SESSION_ID, true)).joinBlocking()

        // then
        verify(mockStore).dispatch(
            UpdatePermissionHighlightsStateAction.Reset(SESSION_ID),
        )
        verify(mockStorage).clearTemporaryPermissions()
    }

    @Test
    fun `GIVEN a tab load after stop is called THEN none stale permission indicators should be clear up`() {
        sitePermissionFeature.start()

        verify(sitePermissionFeature).setupLoadingCollector()

        sitePermissionFeature.stop()

        // when
        mockStore.dispatch(ContentAction.UpdateLoadingStateAction(SESSION_ID, true)).joinBlocking()

        // then
        verify(mockStore, never()).dispatch(
            UpdatePermissionHighlightsStateAction.Reset(SESSION_ID),
        )
    }

    @Test
    fun `GIVEN a sessionId WHEN calling consumePermissionRequest THEN dispatch ConsumePermissionsRequest action with given id`() {
        // when
        sitePermissionFeature.consumePermissionRequest(mockPermissionRequest, "sessionIdTest")

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumePermissionsRequest
            ("sessionIdTest", mockPermissionRequest),
        )
    }

    @Test
    fun `GIVEN no sessionId WHEN calling consumePermissionRequest THEN dispatch ConsumePermissionsRequest action with selected tab`() {
        // when
        sitePermissionFeature.consumePermissionRequest(mockPermissionRequest)

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumePermissionsRequest
            (selectedTab.id, mockPermissionRequest),
        )
    }

    @Test
    fun `GIVEN a sessionId WHEN calling consumeAppPermissionRequest THEN dispatch ConsumeAppPermissionsRequest action with given id`() {
        // when
        sitePermissionFeature.consumeAppPermissionRequest(mockAppPermissionRequest, "sessionIdTest")

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumeAppPermissionsRequest
            ("sessionIdTest", mockAppPermissionRequest),
        )
    }

    @Test
    fun `GIVEN no sessionId WHEN calling consumeAppPermissionRequest THEN dispatch ConsumeAppPermissionsRequest action with selected tab`() {
        // when
        sitePermissionFeature.consumeAppPermissionRequest(mockAppPermissionRequest)

        // then
        verify(mockStore).dispatch(
            ContentAction.ConsumeAppPermissionsRequest
            (selectedTab.id, mockAppPermissionRequest),
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
            arrayOf(PERMISSION_DENIED, PERMISSION_DENIED).toIntArray(),
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
            anyString(),
        )

        // when
        sitePermissionFeature.onPositiveButtonPress(PERMISSION_ID, SESSION_ID, true)

        // then
        verify(sitePermissionFeature)
            .consumePermissionRequest(mockPermissionRequest, SESSION_ID)
        verify(sitePermissionFeature)
            .onContentPermissionGranted(mockPermissionRequest, true)
    }

    @Test
    fun `GIVEN a permission prompt WHEN one permission is allowed THEN emit a fact`() {
        CollectionProcessor.withFactCollection { facts ->
            doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
            doNothing().`when`(sitePermissionFeature)
                .onContentPermissionGranted(mockPermissionRequest, true)
            doReturn(listOf(ContentCrossOriginStorageAccess())).`when`(mockPermissionRequest).permissions
            doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(anyString())

            sitePermissionFeature.onPositiveButtonPress(PERMISSION_ID, SESSION_ID, true)

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CONFIRM, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(ContentCrossOriginStorageAccess().id, facts[0].value)
        }
    }

    @Test
    fun `GIVEN a permission prompt WHEN multiple permission are allowed THEN emit a fact`() {
        CollectionProcessor.withFactCollection { facts ->
            doReturn(
                listOf(ContentVideoCapture(), ContentVideoCamera(), ContentAudioMicrophone()),
            ).`when`(mockPermissionRequest).permissions
            doReturn(true).`when`(mockPermissionRequest).containsVideoAndAudioSources()
            doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(anyString())

            sitePermissionFeature.onPositiveButtonPress(PERMISSION_ID, SESSION_ID, true)

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CONFIRM, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(
                listOf(ContentVideoCapture(), ContentVideoCamera(), ContentAudioMicrophone()).joinToString { it.id!! },
                facts[0].value,
            )
        }
    }

    @Test
    fun `GIVEN permissionRequest WHEN onNegativeButtonPress() THEN consumePermissionRequest, onContentPermissionDeny are called`() {
        // given
        doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
        doNothing().`when`(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, true)
        doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(
            anyString(),
        )

        // when
        sitePermissionFeature.onNegativeButtonPress(PERMISSION_ID, SESSION_ID, true)

        // then
        verify(sitePermissionFeature)
            .consumePermissionRequest(mockPermissionRequest, SESSION_ID)
        verify(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, true)
    }

    @Test
    fun `GIVEN a permission prompt WHEN the permission is denied THEN emit a fact`() {
        CollectionProcessor.withFactCollection { facts ->
            doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
            doNothing().`when`(sitePermissionFeature)
                .onContentPermissionDeny(mockPermissionRequest, true)
            doReturn(listOf(ContentGeoLocation())).`when`(mockPermissionRequest).permissions
            doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(anyString())

            sitePermissionFeature.onNegativeButtonPress(PERMISSION_ID, SESSION_ID, true)

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CANCEL, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(ContentGeoLocation().id, facts[0].value)
        }
    }

    @Test
    fun `GIVEN a permission prompt WHEN multiple permissions are denied THEN emit a fact`() {
        CollectionProcessor.withFactCollection { facts ->
            doReturn(
                listOf(ContentVideoCapture(), ContentVideoCamera(), ContentAudioMicrophone()),
            ).`when`(mockPermissionRequest).permissions
            doReturn(true).`when`(mockPermissionRequest).containsVideoAndAudioSources()
            doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(anyString())

            sitePermissionFeature.onNegativeButtonPress(PERMISSION_ID, SESSION_ID, true)

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.CANCEL, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(
                listOf(ContentVideoCapture(), ContentVideoCamera(), ContentAudioMicrophone()).joinToString { it.id!! },
                facts[0].value,
            )
        }
    }

    @Test
    fun `GIVEN permissionRequest WHEN onDismiss() THEN consumePermissionRequest, onContentPermissionDeny are called`() {
        // given
        doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
        doNothing().`when`(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, false)
        doReturn(mockPermissionRequest).`when`(sitePermissionFeature).findRequestedPermission(
            anyString(),
        )

        // when
        sitePermissionFeature.onDismiss(PERMISSION_ID, SESSION_ID)

        // then
        verify(sitePermissionFeature)
            .consumePermissionRequest(mockPermissionRequest, SESSION_ID)
        verify(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, false)
    }

    @Test
    fun `GIVEN permissionRequest WHEN onLearnMorePress() THEN consumePermissionRequest, onContentPermissionDeny are called and SelectOrAddUseCase is used`() {
        // given
        val permission: ContentCrossOriginStorageAccess = mock()
        val permissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(permission))
        }
        doNothing().`when`(sitePermissionFeature).consumePermissionRequest(any(), any())
        doNothing().`when`(sitePermissionFeature)
            .onContentPermissionDeny(mockPermissionRequest, true)
        doReturn(permissionRequest).`when`(sitePermissionFeature).findRequestedPermission(
            anyString(),
        )
        doReturn(mock<SelectOrAddUseCase>()).`when`(sitePermissionFeature).selectOrAddUseCase

        // when
        sitePermissionFeature.onLearnMorePress(PERMISSION_ID, SESSION_ID)

        // then
        verify(sitePermissionFeature)
            .consumePermissionRequest(permissionRequest, SESSION_ID)
        verify(sitePermissionFeature)
            .onContentPermissionDeny(permissionRequest, false)
        verify(sitePermissionFeature.selectOrAddUseCase).invoke(
            url = STORAGE_ACCESS_DOCUMENTATION_URL,
            private = false,
            source = SessionState.Source.Internal.TextSelection,
        )
    }

    @Test
    fun `GIVEN a new permissionRequest WHEN storeSitePermissions() THEN save(permissionRequest) is called`() = runTestOnMain {
        // given
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        doReturn(null).`when`(mockStorage).findSitePermissionsBy(ArgumentMatchers.anyString(), anyBoolean())
        doNothing().`when`(sitePermissionFeature).updatePermissionToolbarIndicator(
            any(),
            any(),
            anyBoolean(),
        )
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(Permission.AppAudio(id = "permission")))
            whenever(uri).thenReturn(URL)
        }
        doReturn(sitePermissions).`when`(sitePermissionFeature)
            .updateSitePermissionsStatus(any(), any(), any())

        // when
        sitePermissionFeature.storeSitePermissions(
            mockContentState,
            mockPermissionRequest,
            ALLOWED,
            scope,
        )

        // then
        verify(mockStorage).save(sitePermissions, mockPermissionRequest)
        verify(sitePermissionFeature).updatePermissionToolbarIndicator(
            mockPermissionRequest,
            ALLOWED,
            true,
        )
    }

    @Test
    fun `GIVEN an already saved permissionRequest WHEN storeSitePermissions() THEN update(permissionRequest) is called`() = runTestOnMain {
        // given
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        doReturn(sitePermissions).`when`(mockStorage)
            .findSitePermissionsBy(ArgumentMatchers.anyString(), anyBoolean())
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(uri).thenReturn(URL)
            whenever(permissions).thenReturn(listOf(AppAudio(id = "permission")))
        }
        doReturn(sitePermissions).`when`(sitePermissionFeature)
            .updateSitePermissionsStatus(any(), any(), any())

        // when
        sitePermissionFeature.storeSitePermissions(
            mockContentState,
            mockPermissionRequest,
            ALLOWED,
            scope,
        )

        // then
        verify(mockStorage).update(eq(sitePermissions))
    }

    @Test
    fun `GIVEN a permissionRequest WITH a private tab WHEN storeSitePermissions() THEN save or update MUST NOT BE called`() = runTestOnMain {
        // then
        sitePermissionFeature.storeSitePermissions(
            selectedTab.content.copy(private = true),
            mockPermissionRequest,
            ALLOWED,
            scope,
        )

        // when
        verify(mockStorage, never()).save(any(), any())
        verify(mockStorage, never()).update(any())
    }

    @Test
    fun `GIVEN a permanent permissionRequest WHEN onContentPermissionDeny THEN reject(), storeSitePermissions are called`() {
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
            BLOCKED,
        )
    }

    @Test
    fun `GIVEN a temporary permissionRequest WHEN denying THEN store it on memory`() {
        doNothing().`when`(mockPermissionRequest).reject()
        doNothing().`when`(sitePermissionFeature).storeSitePermissions(any(), any(), any(), any())

        sitePermissionFeature.onContentPermissionDeny(mockPermissionRequest, shouldStore = false)

        verify(mockPermissionRequest).reject()
        verify(mockStorage).saveTemporary(mockPermissionRequest)

        verify(sitePermissionFeature, never()).storeSitePermissions(
            selectedTab.content,
            mockPermissionRequest,
            BLOCKED,
        )
    }

    @Test
    fun `GIVEN a temporary permissionRequest WHEN granting THEN store it on memory`() {
        doNothing().`when`(mockPermissionRequest).reject()
        doNothing().`when`(sitePermissionFeature).storeSitePermissions(any(), any(), any(), any())

        sitePermissionFeature.onContentPermissionGranted(mockPermissionRequest, shouldStore = false)

        verify(mockPermissionRequest).grant()
        verify(mockStorage).saveTemporary(mockPermissionRequest)

        verify(sitePermissionFeature, never()).storeSitePermissions(
            selectedTab.content,
            mockPermissionRequest,
            ALLOWED,
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
        runTestOnMain {
            sitePermissionFeature.onContentPermissionRequested(mockPermissionRequest, URL)
        }

        // then
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
    }

    @Test
    fun `GIVEN location permissionRequest and shouldApplyRules is true WHEN onContentPermissionRequested() THEN handleRuledFlow is called`() = runTestOnMain {
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
        runTestOnMain {
            sitePermissionFeature.onContentPermissionRequested(
                mockPermissionRequest,
                URL,
                scope,
            )
        }

        // then
        verify(mockStorage).findSitePermissionsBy(URL)
        verify(sitePermissionFeature).handleRuledFlow(mockPermissionRequest, URL)
    }

    @Test
    fun `GIVEN location permissionRequest and shouldApplyRules is false WHEN onContentPermissionRequested() THEN handleNoRuledFlow is called`() = runTestOnMain {
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
        runTestOnMain {
            sitePermissionFeature.onContentPermissionRequested(
                mockPermissionRequest,
                URL,
                scope,
            )
        }

        // then
        verify(mockStorage).findSitePermissionsBy(URL)
        verify(sitePermissionFeature).handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)
    }

    @Test
    fun `GIVEN autoplay permissionRequest and shouldApplyRules is false WHEN onContentPermissionRequested() THEN handleNoRuledFlow is called`() = runTestOnMain {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAutoPlayInaudible(id = "permission")))
        }
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0)
        val sitePermissionsDialogFragment = SitePermissionsDialogFragment()

        doReturn(sitePermissions).`when`(mockStorage).findSitePermissionsBy(URL)
        doReturn(false).`when`(sitePermissionFeature).shouldApplyRules(any())
        doReturn(sitePermissionsDialogFragment).`when`(sitePermissionFeature)
            .handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)

        // when
        runTestOnMain {
            sitePermissionFeature.onContentPermissionRequested(
                mockPermissionRequest,
                URL,
                scope,
            )
        }

        // then
        verify(mockStorage).findSitePermissionsBy(URL)
        verify(sitePermissionFeature).handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)
    }

    @Test
    fun `GIVEN shouldShowPrompt with isForAutoplay false AND null permissionFromStorage THEN return true`() = runTestOnMain {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(Permission.ContentGeoLocation(id = "permission")))
        }

        // when
        val shouldShowPrompt = sitePermissionFeature.shouldShowPrompt(mockPermissionRequest, null)

        // then
        assertTrue(shouldShowPrompt)
    }

    @Test
    fun `GIVEN shouldShowPrompt with isForAutoplay true THEN return false`() {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(Permission.ContentAutoPlayInaudible(id = "permission")))
        }

        // when
        val shouldShowPrompt = sitePermissionFeature.shouldShowPrompt(mockPermissionRequest, mock())

        // then
        assertFalse(shouldShowPrompt)
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
        doNothing().`when`(sitePermissionFeature).updatePermissionToolbarIndicator(mockPermissionRequest, BLOCKED)

        // when
        sitePermissionFeature.handleNoRuledFlow(null, mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
        verify(sitePermissionFeature).updatePermissionToolbarIndicator(
            mockPermissionRequest,
            BLOCKED,
        )
    }

    @Test
    fun `GIVEN AutoplayAudible request WHEN calling updatePermissionToolbarIndicator THEN dispatch AutoPlayAudibleBlockingAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAutoPlayAudible(id = "permission")))
        }
        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED)

        verify(mockStore).dispatch(AutoPlayAudibleBlockingAction(tab1.id, true))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED)

        verify(mockStore).dispatch(AutoPlayAudibleBlockingAction(tab1.id, false))
    }

    @Test
    fun `GIVEN AutoplayInaudible request WHEN calling updatePermissionToolbarIndicator THEN dispatch AutoPlayInAudibleBlockingAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAutoPlayInaudible(id = "permission")))
        }
        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED)

        verify(mockStore).dispatch(AutoPlayInAudibleBlockingAction(tab1.id, true))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED)

        verify(mockStore).dispatch(AutoPlayInAudibleBlockingAction(tab1.id, false))
    }

    @Test
    fun `GIVEN notification request WHEN calling updatePermissionToolbarIndicator THEN dispatch NotificationChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentNotification(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules).notification

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, false)

        verify(mockStore, never()).dispatch(any<NotificationChangedAction>())

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(NotificationChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(NotificationChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN camera request WHEN calling updatePermissionToolbarIndicator THEN dispatch CameraChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentVideoCamera(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules).camera

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, false)

        verify(mockStore, never()).dispatch(any<CameraChangedAction>())

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(CameraChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(CameraChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN location request WHEN calling updatePermissionToolbarIndicator THEN dispatch CameraChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules).location

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, false)

        verify(mockStore, never()).dispatch(any<LocationChangedAction>())

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(LocationChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(LocationChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN microphone request WHEN calling updatePermissionToolbarIndicator THEN dispatch MicrophoneChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAudioCapture(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules).microphone

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, false)

        verify(mockStore, never()).dispatch(any<MicrophoneChangedAction>())

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(MicrophoneChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(MicrophoneChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN persistentStorage request WHEN calling updatePermissionToolbarIndicator THEN dispatch PersistentStorageChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentPersistentStorage(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules).persistentStorage

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, false)

        verify(mockStore, never()).dispatch(any<PersistentStorageChangedAction>())

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(PersistentStorageChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(PersistentStorageChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN mediaKeySystemAccess request WHEN calling updatePermissionToolbarIndicator THEN dispatch MediaKeySystemAccesChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentMediaKeySystemAccess(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules).mediaKeySystemAccess

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, false)

        verify(mockStore, never()).dispatch(any<MediaKeySystemAccesChangedAction>())

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(MediaKeySystemAccesChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(MediaKeySystemAccesChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN autoplayAudible request WHEN calling updatePermissionToolbarIndicator THEN dispatch AutoPlayAudibleChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAutoPlayAudible(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.AutoplayAction.BLOCKED).`when`(mockSitePermissionRules).autoplayAudible

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(AutoPlayAudibleChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(AutoPlayAudibleChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN autoplayInaudible request WHEN calling updatePermissionToolbarIndicator THEN dispatch AutoPlayInAudibleChangedAction`() {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val request: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAutoPlayInaudible(id = "permission")))
        }

        doReturn(tab1).`when`(sitePermissionFeature).getCurrentTabState()
        doReturn(SitePermissionsRules.AutoplayAction.BLOCKED).`when`(mockSitePermissionRules).autoplayInaudible

        sitePermissionFeature.updatePermissionToolbarIndicator(request, BLOCKED, true)

        verify(mockStore).dispatch(AutoPlayInAudibleChangedAction(tab1.id, false))

        sitePermissionFeature.updatePermissionToolbarIndicator(request, ALLOWED, true)

        verify(mockStore).dispatch(AutoPlayInAudibleChangedAction(tab1.id, true))
    }

    @Test
    fun `GIVEN shouldShowPrompt false and permissionFromStorage granted WHEN handleNoRuledFlow THEN grant, consumePermissionRequest are called `() {
        // given
        val sitePermissions = SitePermissions(origin = "origin", savedAt = 0, location = ALLOWED)
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
        }
        doNothing().`when`(sitePermissionFeature)
            .updatePermissionToolbarIndicator(mockPermissionRequest, ALLOWED, true)

        doReturn(false).`when`(sitePermissionFeature)
            .shouldShowPrompt(mockPermissionRequest, sitePermissions)

        // when
        sitePermissionFeature.handleNoRuledFlow(sitePermissions, mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest, atLeastOnce()).grant()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
        verify(sitePermissionFeature).updatePermissionToolbarIndicator(mockPermissionRequest, ALLOWED, true)
    }

    @Test
    fun `GIVEN permissionRequest with isForAutoplay true and BLOCKED WHEN handleRuledFlow THEN reject, consumePermissionRequest are called`() {
        // given
        val mockPermissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(ContentAutoPlayAudible(id = "permission")))
        }
        doNothing().`when`(sitePermissionFeature)
            .updatePermissionToolbarIndicator(mockPermissionRequest, BLOCKED)
        doReturn(mockSitePermissionRules).`when`(sitePermissionFeature).sitePermissionsRules
        doReturn(SitePermissionsRules.Action.BLOCKED).`when`(mockSitePermissionRules)
            .getActionFrom(mockPermissionRequest)

        // when
        sitePermissionFeature.handleRuledFlow(mockPermissionRequest, URL)

        // then
        verify(mockPermissionRequest).reject()
        verify(sitePermissionFeature).consumePermissionRequest(mockPermissionRequest)
        verify(sitePermissionFeature).updatePermissionToolbarIndicator(
            mockPermissionRequest,
            BLOCKED,
        )
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
            mockPermissionRequest,
            ContentGeoLocation(id = "permission"),
            URL,
        )
    }

    @Test
    fun `GIVEN a ContentStorageAccess request WHEN handlingSingleContentPermissions is called THEN create a specific prompt`() {
        // given
        val host = "https://mozilla.org"
        val permission = ContentCrossOriginStorageAccess(id = "permission")
        val permissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(permission))
            whenever(id).thenReturn("id")
        }

        // when
        sitePermissionFeature.handlingSingleContentPermissions(permissionRequest, permission, host)

        // then
        verify(sitePermissionFeature).createContentCrossOriginStorageAccessPermissionPrompt(
            context = testContext,
            host,
            permissionRequest,
            false,
            true,
        )
    }

    @Test
    fun `GIVEN a ContentStorageAccess request WHEN createContentStorageAccessPermissionPrompt is called THEN create a specific SitePermissionsDialogFragment`() {
        // given
        val host = "https://mozilla.org"
        val permission = ContentCrossOriginStorageAccess(id = "permission")
        val permissionRequest: PermissionRequest = mock {
            whenever(permissions).thenReturn(listOf(permission))
            whenever(id).thenReturn("id")
        }

        // when
        val dialog = sitePermissionFeature.createContentCrossOriginStorageAccessPermissionPrompt(
            testContext,
            host,
            permissionRequest,
            false,
            true,
        )

        // then
        assertEquals(SESSION_ID, dialog.sessionId)
        assertEquals(
            testContext.getString(
                R.string.mozac_feature_sitepermissions_storage_access_title,
                host.stripDefaultPort(),
                selectedTab.content.url.stripDefaultPort(),
            ),
            dialog.title,
        )
        assertEquals(R.drawable.mozac_ic_cookies, dialog.icon)
        assertEquals(permissionRequest.id, dialog.permissionRequestId)
        assertEquals(sitePermissionFeature, dialog.feature)
        assertEquals(false, dialog.shouldShowDoNotAskAgainCheckBox)
        assertEquals(true, dialog.shouldPreselectDoNotAskAgainCheckBox)
        assertEquals(false, dialog.isNotificationRequest)
        assertEquals(
            testContext.getString(
                R.string.mozac_feature_sitepermissions_storage_access_message,
                host.stripDefaultPort(),
            ),
            dialog.message,
        )
        assertEquals(
            testContext.getString(R.string.mozac_feature_sitepermissions_storage_access_not_allow),
            dialog.negativeButtonText,
        )
        assertEquals(true, dialog.shouldShowLearnMoreLink)
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
                    ContentAudioMicrophone(),
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
                ArgumentMatchers.anyBoolean(),
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
            ArgumentMatchers.anyBoolean(),
        )
    }

    @Test
    fun `GIVEN a request for one permission WHEN a prompt is created THEN emit a fact`() {
        CollectionProcessor.withFactCollection { facts ->
            val sitePermissionsDialogFragment = SitePermissionsDialogFragment()
            val mockPermissionRequest: PermissionRequest = mock {
                whenever(permissions).thenReturn(listOf(ContentGeoLocation(id = "permission")))
            }
            doReturn(sitePermissionsDialogFragment).`when`(sitePermissionFeature)
                .handlingSingleContentPermissions(any(), any(), any())

            sitePermissionFeature.createPrompt(mockPermissionRequest, URL)

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.DISPLAY, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals("ContentGeoLocation", facts[0].value)
        }
    }

    @Test
    fun `GIVEN a request for a permission with video and audio sources WHEN a prompt is created THEN emit a fact`() {
        CollectionProcessor.withFactCollection { facts ->
            val permissionRequest: PermissionRequest = object : PermissionRequest {
                override val uri = "http://www.mozilla.org"
                override val id = PERMISSION_ID

                override val permissions: List<Permission>
                    get() = listOf(ContentVideoCapture(), ContentVideoCamera(), ContentAudioMicrophone())

                override fun grant(permissions: List<Permission>) {}

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
                    ArgumentMatchers.anyBoolean(),
                )

            sitePermissionFeature.createPrompt(permissionRequest, URL)

            assertEquals(1, facts.size)
            assertEquals(FEATURE_SITEPERMISSIONS, facts[0].component)
            assertEquals(Action.DISPLAY, facts[0].action)
            assertEquals(SitePermissionsFacts.Items.PERMISSIONS, facts[0].item)
            assertEquals(permissionRequest.permissions.joinToString { it.id!! }, facts[0].value)
        }
    }

    @Test
    fun `is SitePermission granted in the storage`() = runTestOnMain {
        val sitePermissionsList = listOf(
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone(),
            ContentVideoCamera(),
            ContentVideoCapture(),
            ContentPersistentStorage(),
            ContentAutoPlayAudible(),
            ContentAutoPlayInaudible(),
            ContentMediaKeySystemAccess(),
        )

        sitePermissionsList.forEach { permission ->
            val request: PermissionRequest = mock()
            val sitePermissionFromStorage: SitePermissions = mock()

            doReturn(listOf(permission)).`when`(request).permissions
            doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString(), anyBoolean())
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).location
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).notification
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).camera
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).microphone
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).localStorage
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).crossOriginStorageAccess
            doReturn(ALLOWED).`when`(sitePermissionFromStorage).mediaKeySystemAccess
            doReturn(AutoplayStatus.ALLOWED).`when`(sitePermissionFromStorage).autoplayAudible
            doReturn(AutoplayStatus.ALLOWED).`when`(sitePermissionFromStorage).autoplayInaudible

            val isAllowed = sitePermissionFromStorage.isGranted(request)
            assertTrue(isAllowed)
        }
    }

    @Test
    fun `is SitePermission blocked in the storage`() = runTestOnMain {
        val sitePermissionsList = listOf(
            ContentGeoLocation(),
            ContentNotification(),
            ContentAudioCapture(),
            ContentAudioMicrophone(),
            ContentVideoCamera(),
            ContentVideoCapture(),
            ContentCrossOriginStorageAccess(),
            Generic(),
        )

        var exceptionThrown = false
        sitePermissionsList.forEach { permission ->
            val request: PermissionRequest = mock()
            val sitePermissionFromStorage: SitePermissions = mock()

            doReturn(listOf(permission)).`when`(request).permissions
            doReturn(sitePermissionFromStorage).`when`(mockStorage).findSitePermissionsBy(anyString(), anyBoolean())
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).location
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).notification
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).camera
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).microphone
            doReturn(BLOCKED).`when`(sitePermissionFromStorage).crossOriginStorageAccess

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
    fun `getInitialSitePermissions - WHEN sitePermissionsRules is present the function MUST use the sitePermissionsRules values to create a SitePermissions object`() = runTestOnMain {
        val rules = SitePermissionsRules(
            location = SitePermissionsRules.Action.BLOCKED,
            camera = SitePermissionsRules.Action.ASK_TO_ALLOW,
            notification = SitePermissionsRules.Action.ASK_TO_ALLOW,
            microphone = SitePermissionsRules.Action.BLOCKED,
            autoplayAudible = SitePermissionsRules.AutoplayAction.BLOCKED,
            autoplayInaudible = SitePermissionsRules.AutoplayAction.ALLOWED,
            persistentStorage = SitePermissionsRules.Action.BLOCKED,
            crossOriginStorageAccess = SitePermissionsRules.Action.ALLOWED,
            mediaKeySystemAccess = SitePermissionsRules.Action.ASK_TO_ALLOW,
        )

        sitePermissionFeature.sitePermissionsRules = rules

        val sitePermissions = sitePermissionFeature.getInitialSitePermissions(URL)

        assertEquals(URL, sitePermissions.origin)
        assertEquals(BLOCKED, sitePermissions.location)
        assertEquals(NO_DECISION, sitePermissions.camera)
        assertEquals(NO_DECISION, sitePermissions.notification)
        assertEquals(BLOCKED, sitePermissions.microphone)
        assertEquals(BLOCKED, sitePermissions.autoplayAudible.toStatus())
        assertEquals(ALLOWED, sitePermissions.autoplayInaudible.toStatus())
        assertEquals(BLOCKED, sitePermissions.localStorage)
        assertEquals(ALLOWED, sitePermissions.crossOriginStorageAccess)
        assertEquals(NO_DECISION, sitePermissions.mediaKeySystemAccess)
    }

    @Test
    fun `any media request must be rejected WHEN system permissions are not granted first`() = runTestOnMain {
        val permissions = listOf(
            ContentVideoCapture("", "back camera"),
            ContentVideoCamera("", "front camera"),
            ContentAudioCapture(),
            ContentAudioMicrophone(),
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

            val prompt = sitePermissionFeature
                .onContentPermissionRequested(permissionRequest, URL)
            assertNull(prompt)
            assertFalse(grantWasCalled)
        }

        Unit
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}
