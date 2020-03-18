/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.pm.PackageManager
import assertk.assertAll
import assertk.assertThat
import assertk.assertions.isEqualTo
import assertk.assertions.isFalse
import assertk.assertions.isInstanceOf
import assertk.assertions.isNotNull
import assertk.assertions.isNotSameAs
import assertk.assertions.isSameAs
import assertk.assertions.isTrue
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.getInsecureWebsiteUiValues
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.getPermissionStatus
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.getSecuredWebsiteUiValues
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.toWebsitePermission
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeVisible
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
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

        assertAll {
            assertThat(store).isNotNull()
            assertThat(store.state).isNotNull()
            assertThat(store.state.webInfoState).isNotNull()
            assertThat(store.state.websitePermissionsState).isNotNull()
        }
    }

    @Test
    fun `createWebsiteInfoState constructs a WebsiteInfoState with the right values for a secure connection`() {
        val websiteUrl = "https://host.com/page1"
        val websiteTitle = "Hello"
        val certificateIssuer = "issuer"
        val securedStatus = true

        val state = QuickSettingsFragmentStore.createWebsiteInfoState(websiteUrl, websiteTitle, securedStatus, certificateIssuer)

        assertAll {
            assertThat(state).isNotNull()
            assertThat(state.websiteUrl).isSameAs(websiteUrl)
            assertThat(state.websiteTitle).isSameAs(websiteTitle)
            assertThat(state.securityInfoRes).isEqualTo(secureStringRes)
            assertThat(state.iconRes).isEqualTo(secureDrawableRes)
            assertThat(state.iconTintRes).isEqualTo(secureColorRes)
        }
    }

    @Test
    fun `createWebsiteInfoState constructs a WebsiteInfoState with the right values for an insecure connection`() {
        val websiteUrl = "https://host.com/page1"
        val websiteTitle = "Hello"
        val certificateIssuer = "issuer"
        val securedStatus = false

        val state = QuickSettingsFragmentStore.createWebsiteInfoState(websiteUrl, websiteTitle, securedStatus, certificateIssuer)

        assertAll {
            assertThat(state).isNotNull()
            assertThat(state.websiteUrl).isSameAs(websiteUrl)
            assertThat(state.websiteTitle).isSameAs(websiteTitle)
            assertThat(state.securityInfoRes).isEqualTo(insecureStringRes)
            assertThat(state.iconRes).isEqualTo(insecureDrawableRes)
            assertThat(state.iconTintRes).isEqualTo(insecureColorRes)
        }
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

        val state = QuickSettingsFragmentStore.createWebsitePermissionState(
            context, permissions, appSettings
        )

        // Just need to know that the WebsitePermissionsState properties are initialized.
        // Making sure they are correctly initialized is tested in the `initWebsitePermission` test.
        assertAll {
            assertThat(state).isNotNull()
            assertThat(state.camera).isNotNull()
            assertThat(state.microphone).isNotNull()
            assertThat(state.notification).isNotNull()
            assertThat(state.location).isNotNull()
            assertThat(state.autoplayAudible).isNotNull()
            assertThat(state.autoplayInaudible).isNotNull()
        }
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

        assertAll {
            assertThat(websitePermission).isNotNull()
            assertThat(websitePermission).isInstanceOf(WebsitePermission.Camera::class)
            assertThat(websitePermission.status).isEqualTo(allowedStatus)
            assertThat(websitePermission.isVisible).isTrue()
            assertThat(websitePermission.isEnabled).isTrue()
            assertThat(websitePermission.isBlockedByAndroid).isFalse()
        }
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
        assertAll {
            // Check that we only have a non-null permission status.
            // Having each property calculated in a separate delegate means their correctness is
            // to be tested in that delegated method.
            assertThat(permissionsStatus).isNotNull()
        }
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

            assertAll {
                assertThat(store.state).isNotNull()
                assertThat(store.state).isNotSameAs(initialState)
                assertThat(store.state.websitePermissionsState).isNotSameAs(
                    initialWebsitePermissionsState
                )
                assertThat(store.state.webInfoState).isSameAs(websiteInfoState)

                assertThat(store.state.websitePermissionsState.camera).isNotNull()
                assertThat((store.state.websitePermissionsState.camera as WebsitePermission.Camera).name).isEqualTo(
                    cameraPermissionName
                )
                assertThat(store.state.websitePermissionsState.camera.status).isEqualTo(
                    initialCameraStatus
                )
                assertThat(store.state.websitePermissionsState.camera.isVisible).isEqualTo(
                    defaultVisibilityStatus
                )
                assertThat(store.state.websitePermissionsState.camera.isEnabled).isEqualTo(
                    defaultEnabledStatus
                )
                assertThat(store.state.websitePermissionsState.camera.isBlockedByAndroid).isEqualTo(
                    defaultBlockedByAndroidStatus
                )

                assertThat(store.state.websitePermissionsState.microphone).isNotNull()
                assertThat((store.state.websitePermissionsState.microphone as WebsitePermission.Microphone).name).isEqualTo(
                    microphonePermissionName
                )
                // Only the following two properties must have been changed!
                assertThat(store.state.websitePermissionsState.microphone.status).isEqualTo(
                    updatedMicrophoneStatus
                )
                assertThat(store.state.websitePermissionsState.microphone.isEnabled).isEqualTo(
                    updatedMicrophoneEnabledStatus
                )

                assertThat(store.state.websitePermissionsState.microphone.isVisible).isEqualTo(
                    defaultVisibilityStatus
                )
                assertThat(store.state.websitePermissionsState.microphone.isBlockedByAndroid).isEqualTo(
                    defaultBlockedByAndroidStatus
                )

                assertThat(store.state.websitePermissionsState.notification).isNotNull()
                assertThat((store.state.websitePermissionsState.notification as WebsitePermission.Notification).name).isEqualTo(
                    notificationPermissionName
                )
                assertThat(store.state.websitePermissionsState.notification.status).isEqualTo(
                    initialNotificationStatus
                )
                assertThat(store.state.websitePermissionsState.notification.isVisible).isEqualTo(
                    defaultVisibilityStatus
                )
                assertThat(store.state.websitePermissionsState.notification.isEnabled).isEqualTo(
                    defaultEnabledStatus
                )
                assertThat(store.state.websitePermissionsState.notification.isBlockedByAndroid).isEqualTo(
                    defaultBlockedByAndroidStatus
                )

                assertThat(store.state.websitePermissionsState.location).isNotNull()
                assertThat((store.state.websitePermissionsState.location as WebsitePermission.Location).name).isEqualTo(
                    locationPermissionName
                )
                assertThat(store.state.websitePermissionsState.location.status).isEqualTo(
                    initialLocationStatus
                )
                assertThat(store.state.websitePermissionsState.location.isVisible).isEqualTo(
                    defaultVisibilityStatus
                )
                assertThat(store.state.websitePermissionsState.location.isEnabled).isEqualTo(
                    defaultEnabledStatus
                )
                assertThat(store.state.websitePermissionsState.location.isBlockedByAndroid).isEqualTo(
                    defaultBlockedByAndroidStatus
                )
            }
        }

    @Test
    fun `getSecuredWebsiteUiValues() should return the right values`() {
        val uiValues = getSecuredWebsiteUiValues

        assertAll {
            assertThat(uiValues.first).isEqualTo(secureStringRes)
            assertThat(uiValues.second).isEqualTo(secureDrawableRes)
            assertThat(uiValues.third).isEqualTo(secureColorRes)
        }
    }

    @Test
    fun `getInsecureWebsiteUiValues() should return the right values`() {
        val uiValues = getInsecureWebsiteUiValues

        assertAll {
            assertThat(uiValues.first).isEqualTo(insecureStringRes)
            assertThat(uiValues.second).isEqualTo(insecureDrawableRes)
            assertThat(uiValues.third).isEqualTo(insecureColorRes)
        }
    }
}
