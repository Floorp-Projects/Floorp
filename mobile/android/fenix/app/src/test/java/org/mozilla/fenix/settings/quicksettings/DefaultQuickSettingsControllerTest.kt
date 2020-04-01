/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import assertk.assertAll
import assertk.assertThat
import assertk.assertions.isEqualTo
import assertk.assertions.isInstanceOf
import assertk.assertions.isSameAs
import assertk.assertions.isTrue
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
    private val reportIssue = mockk<() -> Unit>(relaxed = true)
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
        reportSiteIssue = reportIssue,
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
        assertAll {
            assertThat(androidPermissions.isCaptured).isTrue()
            assertThat(androidPermissions.captured).isEqualTo(cameraFeature.androidPermissionsList)
        }
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
        assertThat(sitePermissions.camera).isSameAs(NO_DECISION)
        verifyOrder {
            val permission = sitePermissions.toggle(capture(toggledFeature))
            controller.handlePermissionsChange(permission)
        }
        // We should also modify View's state. Not necessarily as the last operation.
        verify {
            store.dispatch(capture(action))
        }
        assertAll {
            assertThat(toggledFeature.isCaptured).isTrue()
            assertThat(toggledFeature.captured).isSameAs(PhoneFeature.CAMERA)

            assertThat(action.isCaptured).isTrue()
            assertThat(action.captured).isInstanceOf(WebsitePermissionAction.TogglePermission::class)
            assertThat((action.captured as WebsitePermissionAction.TogglePermission).websitePermission)
                .isInstanceOf(websitePermission::class)
        }
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
            reportSiteIssue = reportIssue,
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
        assertAll {
            assertThat(action.isCaptured).isTrue()
            assertThat(action.captured).isInstanceOf(WebsitePermissionAction.TogglePermission::class)
            assertThat((action.captured as WebsitePermissionAction.TogglePermission).websitePermission).isEqualTo(
                permission
            )
            assertThat((action.captured as WebsitePermissionAction.TogglePermission).updatedStatus).isEqualTo(
                permissionStatus
            )
            assertThat((action.captured as WebsitePermissionAction.TogglePermission).updatedEnabledStatus).isEqualTo(
                permissionEnabled
            )
        }
    }

    @Test
    fun `handleAndroidPermissionRequest should request from the injected callback`() {
        val testPermissions = arrayOf("TestPermission")
        val requiredPermissions = slot<Array<String>>()
//        every { requestPermissions(capture(requiredPermissions)) } just Runs

        controller.handleAndroidPermissionRequest(testPermissions)

        verify { requestPermissions(capture(requiredPermissions)) }
        assertAll {
            assertThat(requiredPermissions.isCaptured).isTrue()
            assertThat(requiredPermissions.captured).isEqualTo(testPermissions)
        }
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
            assertAll {
                assertThat(permissions.isCaptured).isTrue()
                assertThat(permissions.captured).isEqualTo(testPermissions)
                assertThat(session.isCaptured).isTrue()
                assertThat(session.captured).isEqualTo(browserSession)
            }
        }

    @Test
    fun `WebsitePermission#getBackingFeature should return the PhoneFeature this permission is mapped from`() {
        val cameraPermission = mockk<WebsitePermission.Camera>()
        val microphonePermission = mockk<WebsitePermission.Microphone>()
        val notificationPermission = mockk<WebsitePermission.Notification>()
        val locationPermission = mockk<WebsitePermission.Location>()

        with(controller) {
            assertAll {
                assertThat(cameraPermission.getBackingFeature()).isSameAs(PhoneFeature.CAMERA)
                assertThat(microphonePermission.getBackingFeature()).isSameAs(PhoneFeature.MICROPHONE)
                assertThat(notificationPermission.getBackingFeature()).isSameAs(PhoneFeature.NOTIFICATION)
                assertThat(locationPermission.getBackingFeature()).isSameAs(PhoneFeature.LOCATION)
            }
        }
    }

    @Test
    fun `PhoneFeature#getCorrespondingPermission should return the WebsitePermission which it maps to`() {
        with(controller) {
            assertAll {
                assertThat(PhoneFeature.CAMERA.getCorrespondingPermission())
                    .isInstanceOf(WebsitePermission.Camera::class)
                assertThat(PhoneFeature.MICROPHONE.getCorrespondingPermission())
                    .isInstanceOf(WebsitePermission.Microphone::class)
                assertThat(PhoneFeature.NOTIFICATION.getCorrespondingPermission())
                    .isInstanceOf(WebsitePermission.Notification::class)
                assertThat(PhoneFeature.LOCATION.getCorrespondingPermission())
                    .isInstanceOf(WebsitePermission.Location::class)
                assertThat(PhoneFeature.AUTOPLAY_AUDIBLE.getCorrespondingPermission())
                    .isInstanceOf(WebsitePermission.AutoplayAudible::class)
                assertThat(PhoneFeature.AUTOPLAY_INAUDIBLE.getCorrespondingPermission())
                    .isInstanceOf(WebsitePermission.AutoplayInaudible::class)
            }
        }
    }
}
