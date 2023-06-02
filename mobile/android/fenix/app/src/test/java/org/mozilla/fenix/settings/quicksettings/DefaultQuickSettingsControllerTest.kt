/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.coVerifyOrder
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.CookieBanners
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.toggle
import org.mozilla.fenix.trackingprotection.CookieBannerUIMode
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class DefaultQuickSettingsControllerTest {
    private val context = spyk(testContext)

    private lateinit var browserStore: BrowserStore
    private lateinit var tab: TabSessionState

    @MockK
    private lateinit var store: QuickSettingsFragmentStore

    @MockK(relaxed = true)
    private lateinit var navController: NavController

    @MockK(relaxed = true)
    private lateinit var sitePermissions: SitePermissions

    @MockK(relaxed = true)
    private lateinit var appSettings: Settings

    @MockK(relaxed = true)
    private lateinit var permissionStorage: PermissionStorage

    @MockK(relaxed = true)
    private lateinit var engine: Engine

    @MockK(relaxed = true)
    private lateinit var reload: SessionUseCases.ReloadUrlUseCase

    @MockK(relaxed = true)
    private lateinit var requestPermissions: (Array<String>) -> Unit

    private lateinit var controller: DefaultQuickSettingsController

    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        tab = createTab("https://mozilla.org")
        browserStore = BrowserStore(BrowserState(tabs = listOf(tab)))
        sitePermissions = SitePermissions(origin = "", savedAt = 123)

        controller = spyk(
            DefaultQuickSettingsController(
                context = context,
                quickSettingsStore = store,
                browserStore = browserStore,
                sessionId = tab.id,
                ioScope = scope,
                navController = navController,
                sitePermissions = sitePermissions,
                settings = appSettings,
                permissionStorage = permissionStorage,
                reload = reload,
                requestRuntimePermissions = requestPermissions,
                engine = engine,
                displayPermissions = {},
            ),
        )
    }

    @Test
    fun `handlePermissionsShown should delegate to an injected parameter`() = runTestOnMain {
        every { testContext.components.core.engine } returns mockk(relaxed = true)
        var displayPermissionsInvoked = false
        createController(
            displayPermissions = {
                displayPermissionsInvoked = true
            },
        ).handlePermissionsShown()

        assertTrue(displayPermissionsInvoked)
    }

    @Test
    fun `handlePermissionToggled blocked by Android should handleAndroidPermissionRequest`() = runTestOnMain {
        val cameraFeature = PhoneFeature.CAMERA
        val websitePermission = mockk<WebsitePermission>()
        every { websitePermission.phoneFeature } returns cameraFeature
        every { websitePermission.isBlockedByAndroid } returns true

        controller.handlePermissionToggled(websitePermission)

        verify {
            controller.handleAndroidPermissionRequest(cameraFeature.androidPermissionsList)
        }
    }

    @Test
    fun `handlePermissionToggled allowed by Android should toggle the permissions and modify View's state`() = runTestOnMain {
        val websitePermission = mockk<WebsitePermission>()
        every { websitePermission.phoneFeature } returns PhoneFeature.CAMERA
        every { websitePermission.isBlockedByAndroid } returns false
        every { store.dispatch(any()) } returns mockk()

        controller.handlePermissionToggled(websitePermission)

        // We want to verify that the Status is toggled and this event is passed to Controller also.
        assertSame(NO_DECISION, sitePermissions.camera)
        verify {
            controller.handlePermissionsChange(sitePermissions.toggle(PhoneFeature.CAMERA))
        }
        // We should also modify View's state. Not necessarily as the last operation.
        verify {
            store.dispatch(
                match { action ->
                    PhoneFeature.CAMERA == (action as WebsitePermissionAction.TogglePermission).updatedFeature
                },
            )
        }
    }

    @Test
    fun `handlePermissionToggled blocked by user should navigate to site permission manager`() = runTestOnMain {
        every { testContext.components.core.engine } returns mockk(relaxed = true)
        val websitePermission = mockk<WebsitePermission>()
        val invalidSitePermissionsController = DefaultQuickSettingsController(
            context = context,
            quickSettingsStore = store,
            browserStore = BrowserStore(),
            ioScope = scope,
            navController = navController,
            sessionId = "123",
            sitePermissions = null,
            settings = appSettings,
            permissionStorage = permissionStorage,
            reload = reload,
            requestRuntimePermissions = requestPermissions,
            displayPermissions = {},
        )

        every { websitePermission.phoneFeature } returns PhoneFeature.CAMERA
        every { websitePermission.isBlockedByAndroid } returns false
        every { navController.navigate(any<NavDirections>()) } just Runs

        invalidSitePermissionsController.handlePermissionToggled(websitePermission)

        verify {
            navController.navigate(
                directionsEq(
                    QuickSettingsSheetDialogFragmentDirections.actionGlobalSitePermissionsManagePhoneFeature(
                        PhoneFeature.CAMERA,
                    ),
                ),
            )
        }
    }

    @Test
    fun `handleAutoplayChanged will add autoplay permission`() = runTestOnMain {
        val autoplayValue = mockk<AutoplayValue.AllowAll>(relaxed = true)

        every { store.dispatch(any()) } returns mockk()
        every { controller.handleAutoplayAdd(any(), any()) } returns Unit

        controller.sitePermissions = null

        controller.handleAutoplayChanged(autoplayValue)

        verify {
            controller.handleAutoplayAdd(any(), any())
            store.dispatch(any())
        }
    }

    @Test
    fun `handleAutoplayChanged will update autoplay permission`() = runTestOnMain {
        val autoplayValue = mockk<AutoplayValue.AllowAll>(relaxed = true)

        every { store.dispatch(any()) } returns mockk()
        every { controller.handleAutoplayAdd(any(), any()) } returns Unit
        every { controller.handlePermissionsChange(any()) } returns Unit
        every { autoplayValue.updateSitePermissions(any()) } returns mockk()

        controller.handleAutoplayChanged(autoplayValue)

        verify {
            autoplayValue.updateSitePermissions(any())
            store.dispatch(any())
        }
    }

    @Test
    fun `handleAndroidPermissionGranted should update the View's state`() = runTestOnMain {
        val featureGranted = PhoneFeature.CAMERA
        val permissionStatus = featureGranted.getActionLabel(context, sitePermissions, appSettings)
        val permissionEnabled =
            featureGranted.shouldBeEnabled(context, sitePermissions, appSettings)
        every { store.dispatch(any()) } returns mockk()

        controller.handleAndroidPermissionGranted(featureGranted)

        verify {
            store.dispatch(
                withArg { action ->
                    action as WebsitePermissionAction.TogglePermission
                    assertEquals(featureGranted, action.updatedFeature)
                    assertEquals(permissionStatus, action.updatedStatus)
                    assertEquals(permissionEnabled, action.updatedEnabledStatus)
                },
            )
        }
    }

    @Test
    fun `handleAndroidPermissionRequest should request from the injected callback`() = runTestOnMain {
        every { testContext.components.core.engine } returns mockk(relaxed = true)
        val testPermissions = arrayOf("TestPermission")

        var requestRuntimePermissionsInvoked = false
        createController(
            requestPermissions = {
                assertArrayEquals(testPermissions, it)
                requestRuntimePermissionsInvoked = true
            },
        ).handleAndroidPermissionRequest(testPermissions)

        assertTrue(requestRuntimePermissionsInvoked)
    }

    @Test
    fun `handlePermissionsChange should store the updated permission and reload webpage`() =
        runTestOnMain {
            val testPermissions = mockk<SitePermissions>()

            controller.handlePermissionsChange(testPermissions)
            advanceUntilIdle()

            coVerifyOrder {
                permissionStorage.updateSitePermissions(testPermissions, tab.content.private)
                reload(tab.id)
            }
        }

    @Test
    fun `handleAutoplayAdd should store the updated permission and reload webpage`() =
        runTestOnMain {
            val testPermissions = mockk<SitePermissions>()

            controller.handleAutoplayAdd(testPermissions, true)
            advanceUntilIdle()

            coVerifyOrder {
                permissionStorage.add(testPermissions, true)
                reload(tab.id)
            }
        }

    @Test
    fun `handleCookieBannerHandlingDetailsClicked should call popBackStack and navigate to details page`() {
        every { context.components.core.store } returns browserStore
        every { store.state.protectionsState } returns mockk(relaxed = true)
        every { context.components.settings } returns appSettings
        every { context.components.settings.toolbarPosition.androidGravity } returns mockk(relaxed = true)

        controller.handleCookieBannerHandlingDetailsClicked()

        verify {
            navController.popBackStack()

            navController.navigate(any<NavDirections>())
        }
        assertNotNull(CookieBanners.visitedPanel.testGetValue())
    }

    @Test
    fun `handleTrackingProtectionToggled should call the right use cases`() = runTestOnMain {
        val trackingProtectionUseCases: TrackingProtectionUseCases = mockk(relaxed = true)
        val sessionUseCases: SessionUseCases = mockk(relaxed = true)

        every { context.components.core.store } returns browserStore
        every { context.components.useCases.trackingProtectionUseCases } returns trackingProtectionUseCases
        every { context.components.useCases.sessionUseCases } returns sessionUseCases
        every { store.dispatch(any()) } returns mockk()

        var isEnabled = true

        controller.handleTrackingProtectionToggled(isEnabled)

        verify {
            trackingProtectionUseCases.removeException(tab.id)
            sessionUseCases.reload.invoke(tab.id)
            store.dispatch(TrackingProtectionAction.ToggleTrackingProtectionEnabled(isEnabled))
        }

        isEnabled = false
        assertNull(TrackingProtection.exceptionAdded.testGetValue())

        controller.handleTrackingProtectionToggled(isEnabled)

        assertNotNull(TrackingProtection.exceptionAdded.testGetValue())
        verify {
            trackingProtectionUseCases.addException(tab.id)
            sessionUseCases.reload.invoke(tab.id)
            store.dispatch(TrackingProtectionAction.ToggleTrackingProtectionEnabled(isEnabled))
        }
    }

    @Test
    fun `handleBlockedItemsClicked should call popBackStack and navigate to the tracking protection panel dialog`() = runTestOnMain {
        every { context.components.core.store } returns browserStore
        every { context.components.settings } returns appSettings
        every { context.components.settings.toolbarPosition.androidGravity } returns mockk(relaxed = true)

        val isTrackingProtectionEnabled = true
        val state = QuickSettingsFragmentStore.createTrackingProtectionState(
            context = context,
            websiteUrl = tab.content.url,
            sessionId = tab.id,
            isTrackingProtectionEnabled = isTrackingProtectionEnabled,
            cookieBannerUIMode = CookieBannerUIMode.ENABLE,
        )

        every { store.state.protectionsState } returns state

        controller.handleTrackingProtectionDetailsClicked()

        verify {
            navController.popBackStack()

            navController.navigate(any<NavDirections>())
        }
    }

    @Test
    fun `WHEN handleConnectionDetailsClicked THEN call popBackStack and navigate to the connection details dialog`() = runTestOnMain {
        every { context.components.core.store } returns browserStore
        every { context.components.settings } returns appSettings
        every { context.components.settings.toolbarPosition.androidGravity } returns mockk(relaxed = true)

        val state = WebsiteInfoState.createWebsiteInfoState(
            websiteUrl = tab.content.url,
            websiteTitle = tab.content.title,
            isSecured = true,
            certificateName = "certificateName",
        )

        every { store.state.webInfoState } returns state

        controller.handleConnectionDetailsClicked()

        verify {
            navController.popBackStack()

            navController.navigate(any<NavDirections>())
        }
    }

    @Test
    fun `WHEN handleClearSiteData THEN call clearSite`() = runTestOnMain {
        controller.handleClearSiteDataClicked("mozilla.org")

        verify {
            engine.clearData(
                host = "mozilla.org",
                data = Engine.BrowsingData.select(
                    Engine.BrowsingData.AUTH_SESSIONS,
                    Engine.BrowsingData.ALL_SITE_DATA,
                ),
            )
        }
    }

    private fun createController(
        requestPermissions: (Array<String>) -> Unit = { _ -> },
        displayPermissions: () -> Unit = {},
    ): DefaultQuickSettingsController {
        return spyk(
            DefaultQuickSettingsController(
                context = context,
                quickSettingsStore = store,
                browserStore = browserStore,
                sessionId = tab.id,
                ioScope = scope,
                navController = navController,
                sitePermissions = sitePermissions,
                settings = appSettings,
                permissionStorage = permissionStorage,
                reload = reload,
                requestRuntimePermissions = requestPermissions,
                displayPermissions = displayPermissions,
            ),
        )
    }
}
