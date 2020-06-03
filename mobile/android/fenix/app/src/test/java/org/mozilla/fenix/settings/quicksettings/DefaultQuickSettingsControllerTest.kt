/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.browser.session.Session
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.toggle
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DefaultQuickSettingsControllerTest {
    private val context = testContext
    private val store = mockk<QuickSettingsFragmentStore>()
    private val coroutinesScope = TestCoroutineScope()
    private val navController = mockk<NavController>(relaxed = true)
    private val browserSession = mockk<Session>()
    private val sitePermissions: SitePermissions = SitePermissions(origin = "", savedAt = 123)
    private val appSettings = mockk<Settings>(relaxed = true)
    private val permissionStorage = mockk<PermissionStorage>(relaxed = true)
    private val reload = mockk<SessionUseCases.ReloadUrlUseCase>(relaxed = true)
    private val addNewTab = mockk<TabsUseCases.AddNewTabUseCase>(relaxed = true)
    private val requestPermissions = mockk<(Array<String>) -> Unit>(relaxed = true)
    private val displayPermissions = mockk<() -> Unit>(relaxed = true)
    private val dismiss = mockk<() -> Unit>(relaxed = true)
    private val controller = DefaultQuickSettingsController(
        context = context,
        quickSettingsStore = store,
        coroutineScope = coroutinesScope,
        navController = navController,
        session = browserSession,
        sitePermissions = sitePermissions,
        settings = appSettings,
        permissionStorage = permissionStorage,
        reload = reload,
        addNewTab = addNewTab,
        requestRuntimePermissions = requestPermissions,
        displayPermissions = displayPermissions,
        dismiss = dismiss
    )

    @Test
    fun `handlePermissionsShown should delegate to an injected parameter`() {
        controller.handlePermissionsShown()

        verify {
            displayPermissions()
        }
    }

    @Test
    fun `handlePermissionToggled blocked by Android should handleAndroidPermissionRequest`() {
        val cameraFeature = PhoneFeature.CAMERA
        val websitePermission = mockk<WebsitePermission.Camera>()
        val androidPermissions = slot<Array<String>>()
        every { websitePermission.isBlockedByAndroid } returns true

        controller.handlePermissionToggled(websitePermission)

        verify {
            controller.handleAndroidPermissionRequest(capture(androidPermissions))
        }

        assertTrue(androidPermissions.isCaptured)
        assertArrayEquals(cameraFeature.androidPermissionsList, androidPermissions.captured)
    }

    @Test
    @Ignore("Disabling because of intermittent failures https://github.com/mozilla-mobile/fenix/issues/8621")
    fun `handlePermissionToggled allowed by Android should toggle the permissions and modify View's state`() {
        val permissionName = "CAMERA"
        val websitePermission = mockk<WebsitePermission.Camera>()
        val toggledFeature = slot<PhoneFeature>()
        val action = slot<WebsitePermissionAction>()
        every { websitePermission.isBlockedByAndroid } returns false
        every { websitePermission.name } returns permissionName
        every { store.dispatch(any()) } returns mockk()
        // For using the SitePermissions.toggle(..) extension method we need a static mock of SitePermissions.
        mockkStatic("org.mozilla.fenix.settings.ExtensionsKt")

        controller.handlePermissionToggled(websitePermission)

        // We want to verify that the Status is toggled and this event is passed to Controller also.
        assertSame(NO_DECISION, sitePermissions.camera)
        verifyOrder {
            val permission = sitePermissions.toggle(capture(toggledFeature))
            controller.handlePermissionsChange(permission)
        }
        // We should also modify View's state. Not necessarily as the last operation.
        verify {
            store.dispatch(capture(action))
        }

        assertTrue(toggledFeature.isCaptured)
        assertSame(PhoneFeature.CAMERA, toggledFeature.captured)

        assertTrue(action.isCaptured)
        assertEquals(WebsitePermissionAction.TogglePermission::class, action.captured::class)
        assertEquals(websitePermission::class,
            (action.captured as WebsitePermissionAction.TogglePermission).websitePermission::class)
    }

    @Test
    fun `handlePermissionToggled blocked by user should navigate to site permission manager`() {
        val websitePermission = mockk<WebsitePermission.Camera>()
        val invalidSitePermissionsController = DefaultQuickSettingsController(
            context = context,
            quickSettingsStore = store,
            coroutineScope = coroutinesScope,
            navController = navController,
            session = browserSession,
            sitePermissions = null,
            settings = appSettings,
            permissionStorage = permissionStorage,
            reload = reload,
            addNewTab = addNewTab,
            requestRuntimePermissions = requestPermissions,
            displayPermissions = displayPermissions,
            dismiss = dismiss
        )

        every { websitePermission.isBlockedByAndroid } returns false
        every { navController.navigate(any<NavDirections>()) } just Runs

        invalidSitePermissionsController.handlePermissionToggled(websitePermission)

        verify {
            navController.navigate(any<NavDirections>())
        }
    }

    @Test
    fun `handleAndroidPermissionGranted should update the View's state`() {
        val featureGranted = PhoneFeature.CAMERA
        val permission = with(controller) {
            featureGranted.getCorrespondingPermission()
        }
        val permissionStatus = featureGranted.getActionLabel(context, sitePermissions, appSettings)
        val permissionEnabled =
            featureGranted.shouldBeEnabled(context, sitePermissions, appSettings)
        val action = slot<QuickSettingsFragmentAction>()
        every { store.dispatch(any()) } returns mockk()

        controller.handleAndroidPermissionGranted(featureGranted)

        verify {
            store.dispatch(capture(action))
        }

        assertTrue(action.isCaptured)
        assertEquals(WebsitePermissionAction.TogglePermission::class, action.captured::class)
        assertEquals(permission, (action.captured as WebsitePermissionAction.TogglePermission).websitePermission)
        assertEquals(permissionStatus, (action.captured as WebsitePermissionAction.TogglePermission).updatedStatus)
        assertEquals(permissionEnabled, (action.captured as WebsitePermissionAction.TogglePermission).updatedEnabledStatus)
    }

    @Test
    fun `handleAndroidPermissionRequest should request from the injected callback`() {
        val testPermissions = arrayOf("TestPermission")
        val requiredPermissions = slot<Array<String>>()
//        every { requestPermissions(capture(requiredPermissions)) } just Runs

        controller.handleAndroidPermissionRequest(testPermissions)

        verify { requestPermissions(capture(requiredPermissions)) }

        assertTrue(requiredPermissions.isCaptured)
        assertArrayEquals(testPermissions, requiredPermissions.captured)
    }

    @Test
    @ExperimentalCoroutinesApi
    @Ignore("Intermittently failing; https://github.com/mozilla-mobile/fenix/issues/8621")
    fun `handlePermissionsChange should store the updated permission and reload webpage`() =
        runBlocking {
            val testPermissions = mockk<SitePermissions>()
            val permissions = slot<SitePermissions>()
            val session = slot<Session>()

            controller.handlePermissionsChange(testPermissions)

            verifyOrder {
                permissionStorage.updateSitePermissions(capture(permissions))
                reload(capture(session))
            }

            assertTrue(permissions.isCaptured)
            assertEquals(testPermissions, permissions.captured)
            assertTrue(session.isCaptured)
            assertEquals(browserSession, session.captured)
        }

    @Test
    fun `WebsitePermission#getBackingFeature should return the PhoneFeature this permission is mapped from`() {
        val cameraPermission = mockk<WebsitePermission.Camera>()
        val microphonePermission = mockk<WebsitePermission.Microphone>()
        val notificationPermission = mockk<WebsitePermission.Notification>()
        val locationPermission = mockk<WebsitePermission.Location>()

        with(controller) {
            assertSame(PhoneFeature.CAMERA, cameraPermission.getBackingFeature())
            assertSame(PhoneFeature.MICROPHONE, microphonePermission.getBackingFeature())
            assertSame(PhoneFeature.NOTIFICATION, notificationPermission.getBackingFeature())
            assertSame(PhoneFeature.LOCATION, locationPermission.getBackingFeature())
        }
    }

    @Test
    fun `PhoneFeature#getCorrespondingPermission should return the WebsitePermission which it maps to`() {
        with(controller) {
            assertEquals(WebsitePermission.Camera::class, PhoneFeature.CAMERA.getCorrespondingPermission()::class)
            assertEquals(WebsitePermission.Microphone::class, PhoneFeature.MICROPHONE.getCorrespondingPermission()::class)
            assertEquals(WebsitePermission.Notification::class, PhoneFeature.NOTIFICATION.getCorrespondingPermission()::class)
            assertEquals(WebsitePermission.Location::class, PhoneFeature.LOCATION.getCorrespondingPermission()::class)
            assertEquals(WebsitePermission.AutoplayAudible::class, PhoneFeature.AUTOPLAY_AUDIBLE.getCorrespondingPermission()::class)
            assertEquals(WebsitePermission.AutoplayInaudible::class, PhoneFeature.AUTOPLAY_INAUDIBLE.getCorrespondingPermission()::class)
        }
    }
}
