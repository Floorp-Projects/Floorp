/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeVisible
import org.mozilla.fenix.utils.Settings

class QuickSettingsFragmentStore(
    initialState: QuickSettingsFragmentState
) : Store<QuickSettingsFragmentState, QuickSettingsFragmentAction>(
    initialState,
    ::quickSettingsFragmentReducer
) {
    companion object {
        private val getSecuredWebsiteUiValues = Triple(
            R.string.quick_settings_sheet_secure_connection,
            R.drawable.mozac_ic_lock,
            R.color.photonGreen50
        )

        private val getInsecureWebsiteUiValues = Triple(
            R.string.quick_settings_sheet_insecure_connection,
            R.drawable.mozac_ic_globe,
            R.color.photonRed50
        )

        @Suppress("LongParameterList")
        fun createStore(
            context: Context,
            websiteUrl: String,
            isSecured: Boolean,
            isTrackingProtectionOn: Boolean,
            permissions: SitePermissions?,
            settings: Settings
        ) = QuickSettingsFragmentStore(
            QuickSettingsFragmentState(
                trackingProtectionState = createTrackingProtectionState(websiteUrl, isTrackingProtectionOn, settings),
                webInfoState = createWebsiteInfoState(websiteUrl, isSecured),
                websitePermissionsState = createWebsitePermissionState(context, permissions, settings)
            )
        )

        private fun createTrackingProtectionState(
            websiteUrl: String,
            isTrackingProtectionOn: Boolean,
            settings: Settings
        ) = TrackingProtectionState(
            isVisible = FeatureFlags.etpCategories.not(),
            isTrackingProtectionEnabledPerApp = settings.shouldUseTrackingProtection,
            websiteUrl = websiteUrl,
            isTrackingProtectionEnabledPerWebsite = isTrackingProtectionOn
        )

        private fun createWebsiteInfoState(
            websiteUrl: String,
            isSecured: Boolean
        ): WebsiteInfoState {
            val (stringRes, iconRes, colorRes) = when (isSecured) {
                true -> getSecuredWebsiteUiValues
                false -> getInsecureWebsiteUiValues
            }
            return WebsiteInfoState(websiteUrl, stringRes, iconRes, colorRes)
        }

        private fun createWebsitePermissionState(
            context: Context,
            permissions: SitePermissions?,
            settings: Settings
        ): WebsitePermissionsState {
            val cameraPermission = PhoneFeature.CAMERA.toWebsitePermission(context, permissions, settings)
            val microphonePermission = PhoneFeature.MICROPHONE.toWebsitePermission(context, permissions, settings)
            val notificationPermission = PhoneFeature.NOTIFICATION.toWebsitePermission(context, permissions, settings)
            val locationPermission = PhoneFeature.LOCATION.toWebsitePermission(context, permissions, settings)
            val shouldBeVisible = cameraPermission.isVisible || microphonePermission.isVisible ||
                    notificationPermission.isVisible || locationPermission.isVisible

            return WebsitePermissionsState(shouldBeVisible, cameraPermission, microphonePermission,
                notificationPermission, locationPermission
            )
        }

        private fun PhoneFeature.toWebsitePermission(
            context: Context,
            permissions: SitePermissions?,
            settings: Settings
        ): WebsitePermission {
            val status = getPermissionStatus(context, permissions, settings)
            val nonexistentPermission: WebsitePermission? = null
            return when (this) {
                PhoneFeature.CAMERA -> WebsitePermission.Camera(
                    status.status, status.isVisible, status.isEnabled, status.isBlockedByAndroid
                )
                PhoneFeature.LOCATION -> WebsitePermission.Location(
                    status.status, status.isVisible, status.isEnabled, status.isBlockedByAndroid
                )
                PhoneFeature.MICROPHONE -> WebsitePermission.Microphone(
                    status.status, status.isVisible, status.isEnabled, status.isBlockedByAndroid
                )
                PhoneFeature.NOTIFICATION -> WebsitePermission.Notification(
                    status.status, status.isVisible, status.isEnabled, status.isBlockedByAndroid
                )
                PhoneFeature.AUTOPLAY -> nonexistentPermission!! // fail-fast
            }
        }

        private fun PhoneFeature.getPermissionStatus(
            context: Context,
            permissions: SitePermissions?,
            settings: Settings
        ) = PermissionStatus(
            status = getActionLabel(context, permissions, settings),
            isVisible = shouldBeVisible(permissions, settings),
            isEnabled = shouldBeEnabled(context, permissions, settings),
            isBlockedByAndroid = !isAndroidPermissionGranted(context)
        )

        private data class PermissionStatus(
            val status: String,
            val isVisible: Boolean,
            val isEnabled: Boolean,
            val isBlockedByAndroid: Boolean
        )
    }
}

// -------------------------------------------------------------------------------------------------
// States
// -------------------------------------------------------------------------------------------------

data class QuickSettingsFragmentState(
    val trackingProtectionState: TrackingProtectionState,
    val webInfoState: WebsiteInfoState,
    val websitePermissionsState: WebsitePermissionsState
) : State

data class TrackingProtectionState(
    val isVisible: Boolean,
    val websiteUrl: String,
    val isTrackingProtectionEnabledPerApp: Boolean,
    val isTrackingProtectionEnabledPerWebsite: Boolean
) : State

data class WebsiteInfoState(
    val websiteUrl: String,
    @StringRes val securityInfoRes: Int,
    @DrawableRes val iconRes: Int,
    @ColorRes val iconTintRes: Int
) : State

data class WebsitePermissionsState(
    val isVisible: Boolean,
    val camera: WebsitePermission,
    val microphone: WebsitePermission,
    val notification: WebsitePermission,
    val location: WebsitePermission
) : State

sealed class WebsitePermission {
    abstract val status: String
    abstract val isVisible: Boolean
    abstract val isEnabled: Boolean
    abstract val isBlockedByAndroid: Boolean

    abstract fun copy(
        status: String = this.status,
        isVisible: Boolean = this.isVisible,
        isEnabled: Boolean = this.isEnabled,
        isBlockedByAndroid: Boolean = this.isBlockedByAndroid
    ): WebsitePermission

    data class Camera(
        override val status: String,
        override val isVisible: Boolean,
        override val isEnabled: Boolean,
        override val isBlockedByAndroid: Boolean,
        val name: String = "Camera" // helps to resolve the overload resolution ambiguity for the copy() method
    ) : WebsitePermission() {
        override fun copy(
            status: String,
            isVisible: Boolean,
            isEnabled: Boolean,
            isBlockedByAndroid: Boolean
        ) = copy(
            status = status,
            isVisible = isVisible,
            isEnabled = isEnabled,
            isBlockedByAndroid = isBlockedByAndroid,
            name = name
        )
    }

    data class Microphone(
        override val status: String,
        override val isVisible: Boolean,
        override val isEnabled: Boolean,
        override val isBlockedByAndroid: Boolean,
        val name: String = "Microphone" // helps to resolve the overload resolution ambiguity for the copy() method
    ) : WebsitePermission() {
        override fun copy(
            status: String,
            isVisible: Boolean,
            isEnabled: Boolean,
            isBlockedByAndroid: Boolean
        ) = copy(
            status = status,
            isVisible = isVisible,
            isEnabled = isEnabled,
            isBlockedByAndroid = isBlockedByAndroid,
            name = name
        )
    }

    data class Notification(
        override val status: String,
        override val isVisible: Boolean,
        override val isEnabled: Boolean,
        override val isBlockedByAndroid: Boolean,
        val name: String = "Notification" // helps to resolve the overload resolution ambiguity for the copy() method
    ) : WebsitePermission() {
        override fun copy(
            status: String,
            isVisible: Boolean,
            isEnabled: Boolean,
            isBlockedByAndroid: Boolean
        ) = copy(
            status = status,
            isVisible = isVisible,
            isEnabled = isEnabled,
            isBlockedByAndroid = isBlockedByAndroid,
            name = name
        )
    }

    data class Location(
        override val status: String,
        override val isVisible: Boolean,
        override val isEnabled: Boolean,
        override val isBlockedByAndroid: Boolean,
        val name: String = "Location" // helps to resolve the overload resolution ambiguity for the copy() method
    ) : WebsitePermission() {
        override fun copy(
            status: String,
            isVisible: Boolean,
            isEnabled: Boolean,
            isBlockedByAndroid: Boolean
        ) = copy(
            status = status,
            isVisible = isVisible,
            isEnabled = isEnabled,
            isBlockedByAndroid = isBlockedByAndroid,
            name = name
        )
    }
}

// -------------------------------------------------------------------------------------------------
// Actions
// -------------------------------------------------------------------------------------------------

sealed class QuickSettingsFragmentAction : Action

sealed class TrackingProtectionAction : QuickSettingsFragmentAction() {
    class TrackingProtectionToggled(val trackingEnabled: Boolean) : TrackingProtectionAction()
}

sealed class WebsiteInfoAction : QuickSettingsFragmentAction()

sealed class WebsitePermissionAction : QuickSettingsFragmentAction() {
    class TogglePermission(
        val websitePermission: WebsitePermission,
        val updatedStatus: String,
        val updatedEnabledStatus: Boolean
    ) : WebsitePermissionAction()
}

// -------------------------------------------------------------------------------------------------
// Reducers
// -------------------------------------------------------------------------------------------------

fun quickSettingsFragmentReducer(
    state: QuickSettingsFragmentState,
    action: QuickSettingsFragmentAction
): QuickSettingsFragmentState {
    return when (action) {
        is TrackingProtectionAction -> state.copy(
            trackingProtectionState = TrackingProtectionStateReducer.reduce(
                state.trackingProtectionState,
                action
            )
        )
        is WebsiteInfoAction -> state.copy(
            webInfoState = WebsiteInfoStateReducer.reduce(
                state.webInfoState,
                action
            )
        )
        is WebsitePermissionAction -> state.copy(
            websitePermissionsState = WebsitePermissionsStateReducer.reduce(
                state.websitePermissionsState,
                action
            )
        )
    }
}

object TrackingProtectionStateReducer {
    fun reduce(
        state: TrackingProtectionState,
        action: TrackingProtectionAction
    ): TrackingProtectionState {
        return when (action) {
            is TrackingProtectionAction.TrackingProtectionToggled -> state.copy(
                isTrackingProtectionEnabledPerWebsite = action.trackingEnabled
            )
        }
    }
}

@Suppress("UNUSED_PARAMETER")
object WebsiteInfoStateReducer {
    fun reduce(
        state: WebsiteInfoState,
        action: WebsiteInfoAction
    ): WebsiteInfoState {
        // There is no possible action that can change this View's state while it is displayed to the user.
        // Everytime the View is recreated it starts with a fresh state. This is the only way to display
        // something different.
        return state
    }
}

object WebsitePermissionsStateReducer {
    fun reduce(
        state: WebsitePermissionsState,
        action: WebsitePermissionAction
    ): WebsitePermissionsState {
        return when (action) {
            is WebsitePermissionAction.TogglePermission -> {
                when (action.websitePermission) {
                    is WebsitePermission.Camera -> state.copy(
                        camera = state.camera.copy(
                            status = action.updatedStatus,
                            isEnabled = action.updatedEnabledStatus
                        )
                    )
                    is WebsitePermission.Microphone -> state.copy(
                        microphone = state.microphone.copy(
                            status = action.updatedStatus,
                            isEnabled = action.updatedEnabledStatus
                        )
                    )
                    is WebsitePermission.Notification -> state.copy(
                        notification = state.notification.copy(
                            status = action.updatedStatus,
                            isEnabled = action.updatedEnabledStatus
                        )
                    )
                    is WebsitePermission.Location -> state.copy(
                        location = state.location.copy(
                            status = action.updatedStatus,
                            isEnabled = action.updatedEnabledStatus
                        )
                    )
                }
            }
        }
    }
}
