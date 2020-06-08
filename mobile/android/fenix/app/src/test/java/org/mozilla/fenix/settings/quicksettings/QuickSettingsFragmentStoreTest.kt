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
        assertEquals(WebsiteSecurityUiValues.SECURE, state.websiteSecurityUiValues)
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
        assertEquals(WebsiteSecurityUiValues.INSECURE, state.websiteSecurityUiValues)
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
        assertNotNull(state[PhoneFeature.CAMERA])
        assertNotNull(state[PhoneFeature.MICROPHONE])
        assertNotNull(state[PhoneFeature.NOTIFICATION])
        assertNotNull(state[PhoneFeature.LOCATION])
        assertNotNull(state[PhoneFeature.AUTOPLAY_AUDIBLE])
        assertNotNull(state[PhoneFeature.AUTOPLAY_INAUDIBLE])
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
        assertEquals(cameraFeature, websitePermission.phoneFeature)
        assertEquals(allowedStatus, websitePermission.status)
        assertTrue(websitePermission.isVisible)
        assertTrue(websitePermission.isEnabled)
        assertFalse(websitePermission.isBlockedByAndroid)
    }

    @Test
    fun `PhoneFeature#getPermissionStatus gets the permission properties from delegates`() {
        val phoneFeature = PhoneFeature.CAMERA
        every { permissions.camera } returns SitePermissions.Status.NO_DECISION

        val permissionsStatus = phoneFeature.toWebsitePermission(context, permissions, appSettings)

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
            val baseWebsitePermission = WebsitePermission(
                phoneFeature = PhoneFeature.CAMERA,
                status = "",
                isVisible = true,
                isEnabled = true,
                isBlockedByAndroid = true
            )
            val initialWebsitePermissionsState = mapOf(
                PhoneFeature.CAMERA to baseWebsitePermission.copy(
                    phoneFeature = PhoneFeature.CAMERA,
                    status = initialCameraStatus
                ),
                PhoneFeature.MICROPHONE to baseWebsitePermission.copy(
                    phoneFeature = PhoneFeature.MICROPHONE,
                    status = initialMicStatus
                ),
                PhoneFeature.NOTIFICATION to baseWebsitePermission.copy(
                    phoneFeature = PhoneFeature.NOTIFICATION,
                    status = initialNotificationStatus
                ),
                PhoneFeature.LOCATION to baseWebsitePermission.copy(
                    phoneFeature = PhoneFeature.LOCATION,
                    status = initialLocationStatus
                ),
                PhoneFeature.AUTOPLAY_AUDIBLE to baseWebsitePermission.copy(
                    phoneFeature = PhoneFeature.AUTOPLAY_AUDIBLE,
                    status = initialAutoplayAudibleStatus
                ),
                PhoneFeature.AUTOPLAY_INAUDIBLE to baseWebsitePermission.copy(
                    phoneFeature = PhoneFeature.AUTOPLAY_INAUDIBLE,
                    status = initialAutoplayInaudibleStatus
                )
            )
            val initialState = QuickSettingsFragmentState(
                websiteInfoState, initialWebsitePermissionsState
            )
            val store = QuickSettingsFragmentStore(initialState)

            store.dispatch(
                WebsitePermissionAction.TogglePermission(
                    PhoneFeature.MICROPHONE,
                    updatedMicrophoneStatus,
                    updatedMicrophoneEnabledStatus
                )
            ).join()

            assertNotNull(store.state)
            assertNotSame(initialState, store.state)
            assertNotSame(initialWebsitePermissionsState, store.state.websitePermissionsState)
            assertSame(websiteInfoState, store.state.webInfoState)

            assertNotNull(store.state.websitePermissionsState[PhoneFeature.CAMERA])
            assertEquals(PhoneFeature.CAMERA, store.state.websitePermissionsState.getValue(PhoneFeature.CAMERA).phoneFeature)
            assertEquals(initialCameraStatus, store.state.websitePermissionsState.getValue(PhoneFeature.CAMERA).status)
            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.getValue(PhoneFeature.CAMERA).isVisible)
            assertEquals(defaultEnabledStatus, store.state.websitePermissionsState.getValue(PhoneFeature.CAMERA).isEnabled)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.getValue(PhoneFeature.CAMERA).isBlockedByAndroid)

            assertNotNull(store.state.websitePermissionsState[PhoneFeature.MICROPHONE])
            assertEquals(PhoneFeature.MICROPHONE, store.state.websitePermissionsState.getValue(PhoneFeature.MICROPHONE).phoneFeature)

            // Only the following two properties must have been changed!
            assertEquals(updatedMicrophoneStatus, store.state.websitePermissionsState.getValue(PhoneFeature.MICROPHONE).status)
            assertEquals(updatedMicrophoneEnabledStatus, store.state.websitePermissionsState.getValue(PhoneFeature.MICROPHONE).isEnabled)

            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.getValue(PhoneFeature.MICROPHONE).isVisible)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.getValue(PhoneFeature.MICROPHONE).isBlockedByAndroid)

            assertNotNull(store.state.websitePermissionsState[PhoneFeature.NOTIFICATION])
            assertEquals(PhoneFeature.NOTIFICATION, store.state.websitePermissionsState.getValue(PhoneFeature.NOTIFICATION).phoneFeature)
            assertEquals(initialNotificationStatus, store.state.websitePermissionsState.getValue(PhoneFeature.NOTIFICATION).status)
            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.getValue(PhoneFeature.NOTIFICATION).isVisible)
            assertEquals(defaultEnabledStatus, store.state.websitePermissionsState.getValue(PhoneFeature.NOTIFICATION).isEnabled)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.getValue(PhoneFeature.NOTIFICATION).isBlockedByAndroid)

            assertNotNull(store.state.websitePermissionsState[PhoneFeature.LOCATION])
            assertEquals(PhoneFeature.LOCATION, store.state.websitePermissionsState.getValue(PhoneFeature.LOCATION).phoneFeature)
            assertEquals(initialLocationStatus, store.state.websitePermissionsState.getValue(PhoneFeature.LOCATION).status)
            assertEquals(defaultVisibilityStatus, store.state.websitePermissionsState.getValue(PhoneFeature.LOCATION).isVisible)
            assertEquals(defaultEnabledStatus, store.state.websitePermissionsState.getValue(PhoneFeature.LOCATION).isEnabled)
            assertEquals(defaultBlockedByAndroidStatus, store.state.websitePermissionsState.getValue(PhoneFeature.LOCATION).isBlockedByAndroid)
        }
}
