/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.pm.PackageManager
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.getInsecureWebsiteUiValues
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.getPermissionStatus
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.getSecuredWebsiteUiValues
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.toWebsitePermission
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeVisible
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_BLOCK_ALL
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class QuickSettingsFragmentStoreTest {
    private val context = spyk(testContext)
    private val permissions = mockk<SitePermissions>()
    private val appSettings = mockk<Settings>()
    private val secureStringRes = R.string.quick_settings_sheet_secure_connection
    private val secureDrawableRes = R.drawable.mozac_ic_lock
    private val secureColorRes = R.color.photonGreen50
    private val insecureStringRes = R.string.quick_settings_sheet_insecure_connection
    private val insecureDrawableRes = R.drawable.mozac_ic_globe
    private val insecureColorRes = R.color.photonRed50

    @Test
    fun `createStore constructs a QuickSettingsFragmentState`() {
        val settings = mockk<Settings>(relaxed = true)
        val permissions = mockk<SitePermissions>(relaxed = true)

        val store = QuickSettingsFragmentStore.createStore(
            context, "url", "Hello", "issuer", true, permissions, settings
        )

        assertNotNull(store)
        assertNotNull(store.state)
        assertNotNull(store.state.webInfoState)
        assertNotNull(store.state.websitePermissionsState)
    }

    @Test
    fun `createWebsiteInfoState constructs a WebsiteInfoState with the right values for a secure connection`() {
        val websiteUrl = "https://host.com/page1"
        val websiteTitle = "Hello"
        val certificateIssuer = "issuer"
        val securedStatus = true

        val state = QuickSettingsFragmentStore.createWebsiteInfoState(websiteUrl, websiteTitle, securedStatus, certificateIssuer)

        assertNotNull(state)
        assertSame(websiteUrl, state.websiteUrl)
        assertSame(websiteTitle, state.websiteTitle)
        assertEquals(secureStringRes, state.securityInfoRes)
        assertEquals(secureDrawableRes, state.iconRes)
        assertEquals(secureColorRes, state.iconTintRes)
    }

    @Test
    fun `createWebsiteInfoState constructs a WebsiteInfoState with the right values for an insecure connection`() {
        val websiteUrl = "https://host.com/page1"
        val websiteTitle = "Hello"
        val certificateIssuer = "issuer"
        val securedStatus = false

        val state = QuickSettingsFragmentStore.createWebsiteInfoState(websiteUrl, websiteTitle, securedStatus, certificateIssuer)

        assertNotNull(state)
        assertSame(websiteUrl, state.websiteUrl)
        assertSame(websiteTitle, state.websiteTitle)
        assertEquals(insecureStringRes, state.securityInfoRes)
        assertEquals(insecureDrawableRes, state.iconRes)
        assertEquals(insecureColorRes, state.iconTintRes)
    }

    @Test
    fun `createWebsitePermissionState helps in constructing an initial WebsitePermissionState for it's Store`() {
        every {
            context.checkPermission(
                any(),
                any(),
                any()
            )
        }.returns(PackageManager.PERMISSION_GRANTED)
        every { permissions.camera } returns SitePermissions.Status.ALLOWED
        every { permissions.microphone } returns SitePermissions.Status.NO_DECISION
        every { permissions.notification } returns SitePermissions.Status.BLOCKED
        every { permissions.location } returns SitePermissions.Status.ALLOWED
        every { permissions.autoplayAudible } returns SitePermissions.Status.BLOCKED
        every { permissions.autoplayInaudible } returns SitePermissions.Status.BLOCKED
        every { appSettings.getAutoplayUserSetting(any()) } returns AUTOPLAY_BLOCK_ALL

        val state = QuickSettingsFragmentStore.createWebsitePermissionState(
            context, permissions, appSettings
        )

        // Just need to know that the WebsitePermissionsState properties are initialized.
        // Making sure they are correctly initialized is tested in the `initWebsitePermission` test.
        assertNotNull(state)
        assertNotNull(state.camera)
        assertNotNull(state.microphone)
        assertNotNull(state.notification)
        assertNotNull(state.location)
        assertNotNull(state.autoplayAudible)
        assertNotNull(state.autoplayInaudible)
    }

    @Test
    fun `PhoneFeature#toWebsitePermission helps in constructing the right WebsitePermission`() {
        val cameraFeature = PhoneFeature.CAMERA
        val allowedStatus = testContext.getString(R.string.preference_option_phone_feature_allowed)
        every {
            context.checkPermission(
                any(),
                any(),
                any()
            )
        }.returns(PackageManager.PERMISSION_GRANTED)
        every { permissions.camera } returns SitePermissions.Status.ALLOWED

        val websitePermission = cameraFeature.toWebsitePermission(context, permissions, appSettings)

        assertNotNull(websitePermission)
        assertEquals(WebsitePermission.Camera::class, websitePermission::class)
        assertEquals(allowedStatus, websitePermission.status)
        assertTrue(websitePermission.isVisible)
        assertTrue(websitePermission.isEnabled)
        assertFalse(websitePermission.isBlockedByAndroid)
    }

    @Test
    fun `PhoneFeature#getPermissionStatus gets the permission properties from delegates`() {
        val phoneFeature = PhoneFeature.CAMERA
        every { permissions.camera } returns SitePermissions.Status.NO_DECISION

        val permissionsStatus = phoneFeature.getPermissionStatus(context, permissions, appSettings)

        verify {
            // Verifying phoneFeature.getActionLabel gets "Status(child of #2#4).ordinal()) was not called"
//            phoneFeature.getActionLabel(context, permissions, appSettings)
            phoneFeature.shouldBeVisible(permissions, appSettings)
            phoneFeature.shouldBeEnabled(context, permissions, appSettings)
            phoneFeature.isAndroidPermissionGranted(context)
        }

        // Check that we only have a non-null permission status.
        // Having each property calculated in a separate delegate means their correctness is
        // to be tested in that delegated method.
        assertNotNull(permissionsStatus)
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `TogglePermission should only modify status and visibility of a specific WebsitePermissionsState`() =
        runBlocking {
            val cameraPermissionName = "Camera"
            val microphonePermissionName = "Microphone"
            val notificationPermissionName = "Notification"
            val locationPermissionName = "Location"
            val autoplayAudiblePermissionName = "AutoplayAudible"
            val autoplayInaudiblePermissionName = "AutoplayInaudible"
            val initialCameraStatus = "initialCameraStatus"
            val initialMicStatus = "initialMicStatus"
            val initialNotificationStatus = "initialNotificationStatus"
            val initialLocationStatus = "initialLocationStatus"
            val initialAutoplayAudibleStatus = "initialAutoplayAudibleStatus"
            val initialAutoplayInaudibleStatus = "initialAutoplayInaudibleStatus"
            val updatedMicrophoneStatus = "updatedNotificationStatus"
            val updatedMicrophoneEnabledStatus = false
            val defaultVisibilityStatus = true
            val defaultEnabledStatus = true
            val defaultBlockedByAndroidStatus = true
            val websiteInfoState = mockk<WebsiteInfoState>()
            val initialWebsitePermissionsState = WebsitePermissionsState(
                isVisible = true,
                camera = WebsitePermission.Camera(
                    initialCameraStatus, defaultVisibilityStatus,
                    defaultEnabledStatus, defaultBlockedByAndroidStatus, cameraPermissionName
                ),
                microphone = WebsitePermission.Microphone(
                    initialMicStatus, defaultVisibilityStatus,
                    defaultEnabledStatus, defaultBlockedByAndroidStatus, microphonePermissionName
                ),
                notification = WebsitePermission.Notification(
                    initialNotificationStatus, defaultVisibilityStatus,
                    defaultEnabledStatus, defaultBlockedByAndroidStatus, notificationPermissionName
                ),
                location = WebsitePermission.Location(
                    initialLocationStatus, defaultVisibilityStatus,
                    defaultEnabledStatus, defaultBlockedByAndroidStatus, locationPermissionName
                ),
                autoplayAudible = WebsitePermission.AutoplayAudible(
                    initialAutoplayAudibleStatus, defaultVisibilityStatus,
                    defaultEnabledStatus, defaultBlockedByAndroidStatus, autoplayAudiblePermissionName
                ),
                autoplayInaudible = WebsitePermission.AutoplayInaudible(
                    initialAutoplayInaudibleStatus, defaultVisibilityStatus,
                    defaultEnabledStatus, defaultBlockedByAndroidStatus, autoplayInaudiblePermissionName
                )
            )
            val initialState = QuickSettingsFragmentState(
                websiteInfoState, initialWebsitePermissionsState
            )
            val store = QuickSettingsFragmentStore(initialState)

            store.dispatch(
                WebsitePermissionAction.TogglePermission(
                    mockk<WebsitePermission.Microphone>(),
                    updatedMicrophoneStatus,
                    updatedMicrophoneEnabledStatus
                )
            ).join()

            assertNotNull(store.state)
            assertNotSame(initialState, store.state)
            assertNotSame(initialWebsitePermissionsState, store.state.websitePermissionsState)
            assertSame(websiteInfoState, store.state.webInfoState)

            assertNotNull(store.state.websitePermissionsState.camera)
            assertEquals(cameraPermissionName, (store.state.websitePermissionsState.camera as WebsitePermission.Camera).name)
            assertEquals(initialCameraStatus, store.state.websitePermissionsState.camera.status)
            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.camera.isVisible)
            assertEquals(defaultEnabledStatus, store.state.websitePermissionsState.camera.isEnabled)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.camera.isBlockedByAndroid)

            assertNotNull(store.state.websitePermissionsState.microphone)
            assertEquals(microphonePermissionName, (store.state.websitePermissionsState.microphone as WebsitePermission.Microphone).name)

            // Only the following two properties must have been changed!
            assertEquals(updatedMicrophoneStatus, store.state.websitePermissionsState.microphone.status)
            assertEquals(updatedMicrophoneEnabledStatus, store.state.websitePermissionsState.microphone.isEnabled)

            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.microphone.isVisible)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.microphone.isBlockedByAndroid)

            assertNotNull(store.state.websitePermissionsState.notification)
            assertEquals(notificationPermissionName, (store.state.websitePermissionsState.notification as WebsitePermission.Notification).name)
            assertEquals(initialNotificationStatus, store.state.websitePermissionsState.notification.status)
            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.notification.isVisible)
            assertEquals(defaultEnabledStatus, store.state.websitePermissionsState.notification.isEnabled)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.notification.isBlockedByAndroid)

            assertNotNull(store.state.websitePermissionsState.location)
            assertEquals(locationPermissionName, (store.state.websitePermissionsState.location as WebsitePermission.Location).name)
            assertEquals(initialLocationStatus, store.state.websitePermissionsState.location.status)
            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.location.isVisible)
            assertEquals(defaultEnabledStatus, store.state.websitePermissionsState.location.isEnabled)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.location.isBlockedByAndroid)
        }

    @Test
    fun `getSecuredWebsiteUiValues() should return the right values`() {
        val uiValues = getSecuredWebsiteUiValues

        assertEquals(secureStringRes, uiValues.first)
        assertEquals(secureDrawableRes, uiValues.second)
        assertEquals(secureColorRes, uiValues.third)
    }

    @Test
    fun `getInsecureWebsiteUiValues() should return the right values`() {
        val uiValues = getInsecureWebsiteUiValues

        assertEquals(insecureStringRes, uiValues.first)
        assertEquals(insecureDrawableRes, uiValues.second)
        assertEquals(insecureColorRes, uiValues.third)
    }
}
