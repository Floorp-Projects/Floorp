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
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.session.Session
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.toggle
import org.mozilla.fenix.utils.Settings

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
    private val controller = spyk(DefaultQuickSettingsController(
        context = context,
        quickSettingsStore = store,
        ioScope = coroutinesScope,
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
    ))

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
        val websitePermission = mockk<WebsitePermission>()
        every { websitePermission.phoneFeature } returns cameraFeature
        every { websitePermission.isBlockedByAndroid } returns true

        controller.handlePermissionToggled(websitePermission)

        verify {
            controller.handleAndroidPermissionRequest(cameraFeature.androidPermissionsList)
        }
    }

    @Test
    fun `handlePermissionToggled allowed by Android should toggle the permissions and modify View's state`() {
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
            store.dispatch(match { action ->
                PhoneFeature.CAMERA == (action as WebsitePermissionAction.TogglePermission).updatedFeature
            })
        }
    }

    @Test
    fun `handlePermissionToggled blocked by user should navigate to site permission manager`() {
        val websitePermission = mockk<WebsitePermission>()
        val invalidSitePermissionsController = DefaultQuickSettingsController(
            context = context,
            quickSettingsStore = store,
            ioScope = coroutinesScope,
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

        every { websitePermission.phoneFeature } returns PhoneFeature.CAMERA
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
        val permissionStatus = featureGranted.getActionLabel(context, sitePermissions, appSettings)
        val permissionEnabled = featureGranted.shouldBeEnabled(context, sitePermissions, appSettings)
        every { store.dispatch(any()) } returns mockk()

        controller.handleAndroidPermissionGranted(featureGranted)

        verify {
            store.dispatch(withArg { action ->
                action as WebsitePermissionAction.TogglePermission
                assertEquals(featureGranted, action.updatedFeature)
                assertEquals(permissionStatus, action.updatedStatus)
                assertEquals(permissionEnabled, action.updatedEnabledStatus)
            })
        }
    }

    @Test
    fun `handleAndroidPermissionRequest should request from the injected callback`() {
        val testPermissions = arrayOf("TestPermission")

        controller.handleAndroidPermissionRequest(testPermissions)

        verify { requestPermissions(testPermissions) }
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `handlePermissionsChange should store the updated permission and reload webpage`() = coroutinesScope.runBlockingTest {
        val testPermissions = mockk<SitePermissions>()

        controller.handlePermissionsChange(testPermissions)
        advanceUntilIdle()

        verifyOrder {
            permissionStorage.updateSitePermissions(testPermissions)
            reload(browserSession)
        }
    }
}
