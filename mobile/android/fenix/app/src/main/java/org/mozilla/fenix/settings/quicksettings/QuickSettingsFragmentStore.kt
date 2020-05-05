/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Reducer
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.QuickSettingsFragmentStore.Companion.createStore
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeVisible
import org.mozilla.fenix.utils.Settings

/**
 * [QuickSettingsSheetDialogFragment]'s unique [Store].
 * Encompasses it's own:
 *  - [State] for all Views displayed in this Fragment.
 *  - [Action]s mapping a user / system interaction to an intention to modify the above State.
 *  - [Reducer]s for modifying the above State based on the above Actions.
 *
 *  The [createStore] helper method can be used for creating one such [State] based on all current
 *  conditions of the app and web page visited.
 *
 * @param initialState [QuickSettingsFragmentState] that will be shown initially to the user.
 */
class QuickSettingsFragmentStore(
    initialState: QuickSettingsFragmentState
) : Store<QuickSettingsFragmentState, QuickSettingsFragmentAction>(
    initialState,
    ::quickSettingsFragmentReducer
) {
    companion object {
        /**
         * String, Drawable & Drawable Tint color used to display that the current website connection is secured.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        val getSecuredWebsiteUiValues = Triple(
            R.string.quick_settings_sheet_secure_connection,
            R.drawable.mozac_ic_lock,
            R.color.photonGreen50
        )

        /**
         * String, Drawable & Drawable Tint color used to display that the current website connection is
         * **not** secured.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        val getInsecureWebsiteUiValues = Triple(
            R.string.quick_settings_sheet_insecure_connection,
            R.drawable.mozac_ic_globe,
            R.color.photonRed50
        )

        /**
         * Construct an initial [QuickSettingsFragmentState] for all Views displayed by the
         * [QuickSettingsSheetDialogFragment].
         *
         * @param context [Context] used for access to various Android resources.
         * @param websiteUrl [String] the URL of the current web page.
         * @param websiteTitle [String] the title of the current web page.
         * @param isSecured [Boolean] whether the connection is secured (TLS) or not.
         * @param permissions [SitePermissions]? list of website permissions and their status.
         * @param settings [Settings] application settings.
         * @param certificateName [String] the certificate name of the current web  page.
         */
        @Suppress("LongParameterList")
        fun createStore(
            context: Context,
            websiteUrl: String,
            websiteTitle: String,
            certificateName: String,
            isSecured: Boolean,
            permissions: SitePermissions?,
            settings: Settings
        ) = QuickSettingsFragmentStore(
            QuickSettingsFragmentState(
                webInfoState = createWebsiteInfoState(websiteUrl, websiteTitle, isSecured, certificateName),
                websitePermissionsState = createWebsitePermissionState(
                    context,
                    permissions,
                    settings
                )
            )
        )

        /**
         * Construct an initial [WebsiteInfoState] to be rendered by [WebsiteInfoView]
         * based on the current website's status and connection.
         *
         * While being displayed users have no way of modifying it.
         *
         * @param websiteUrl [String] the URL of the current web page.
         * @param isSecured [Boolean] whether the connection is secured (TLS) or not.
         */
        @VisibleForTesting
        fun createWebsiteInfoState(
            websiteUrl: String,
            websiteTitle: String,
            isSecured: Boolean,
            certificateName: String
        ): WebsiteInfoState {
            val (stringRes, iconRes, colorRes) = when (isSecured) {
                true -> getSecuredWebsiteUiValues
                false -> getInsecureWebsiteUiValues
            }
            return WebsiteInfoState(websiteUrl, websiteTitle, stringRes, iconRes, colorRes, certificateName)
        }

        /**
         * Construct an initial [WebsitePermissionsState] to be rendered by [WebsitePermissionsView]
         * containing the permissions requested by the current website.
         *
         * Users can modify the returned [WebsitePermissionsState] after it is initially displayed.
         *
         * @param context [Context] used for various Android interactions.
         * @param permissions [SitePermissions]? list of website permissions and their status.
         * @param settings [Settings] application settings.
         */
        @VisibleForTesting
        fun createWebsitePermissionState(
            context: Context,
            permissions: SitePermissions?,
            settings: Settings
        ): WebsitePermissionsState {
            val cameraPermission =
                PhoneFeature.CAMERA.toWebsitePermission(context, permissions, settings)
            val microphonePermission =
                PhoneFeature.MICROPHONE.toWebsitePermission(context, permissions, settings)
            val notificationPermission =
                PhoneFeature.NOTIFICATION.toWebsitePermission(context, permissions, settings)
            val locationPermission =
                PhoneFeature.LOCATION.toWebsitePermission(context, permissions, settings)
            val autoplayAudiblePermission =
                PhoneFeature.AUTOPLAY_AUDIBLE.toWebsitePermission(context, permissions, settings)
            val autoplayInaudiblePermission =
                PhoneFeature.AUTOPLAY_INAUDIBLE.toWebsitePermission(context, permissions, settings)
            val shouldBeVisible = cameraPermission.isVisible || microphonePermission.isVisible ||
                    notificationPermission.isVisible || locationPermission.isVisible

            return WebsitePermissionsState(
                shouldBeVisible, cameraPermission, microphonePermission,
                notificationPermission, locationPermission, autoplayAudiblePermission,
                autoplayInaudiblePermission
            )
        }

        /**
         * [PhoneFeature] to a [WebsitePermission] mapper.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        fun PhoneFeature.toWebsitePermission(
            context: Context,
            permissions: SitePermissions?,
            settings: Settings
        ): WebsitePermission {
            val status = getPermissionStatus(context, permissions, settings)
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
                PhoneFeature.AUTOPLAY_AUDIBLE -> WebsitePermission.AutoplayAudible(
                    status.status, status.isVisible, status.isEnabled, status.isBlockedByAndroid
                )
                PhoneFeature.AUTOPLAY_INAUDIBLE -> WebsitePermission.AutoplayInaudible(
                    status.status, status.isVisible, status.isEnabled, status.isBlockedByAndroid
                )
            }
        }

        /**
         * Helper method for getting the [WebsitePermission] properties based on a specific [PhoneFeature].
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        fun PhoneFeature.getPermissionStatus(
            context: Context,
            permissions: SitePermissions?,
            settings: Settings
        ) = PermissionStatus(
            status = getActionLabel(context, permissions, settings),
            isVisible = shouldBeVisible(permissions, settings),
            isEnabled = shouldBeEnabled(context, permissions, settings),
            isBlockedByAndroid = !isAndroidPermissionGranted(context)
        )

        /**
         * Helper class acting as a temporary container of [WebsitePermission] properties.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        data class PermissionStatus(
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

/**
 * [State] containing all data displayed to the user by this Fragment.
 *
 * Partitioned further to contain mutiple states for each standalone View this Fragment holds.
 */
data class QuickSettingsFragmentState(
    val webInfoState: WebsiteInfoState,
    val websitePermissionsState: WebsitePermissionsState
) : State

/**
 * [State] to be rendered by [WebsiteInfoView] indicating whether the connection is secure or not.
 *
 * @param websiteUrl [String] the URL of the current web page.
 * @param websiteTitle [String] the title of the current web page.
 * @param securityInfoRes [StringRes] for the connection description.
 * @param iconRes [DrawableRes] image indicating the connection status.
 * @param iconTintRes [ColorRes] icon color.
 */
data class WebsiteInfoState(
    val websiteUrl: String,
    val websiteTitle: String,
    @StringRes val securityInfoRes: Int,
    @DrawableRes val iconRes: Int,
    @ColorRes val iconTintRes: Int,
    val certificateName: String
) : State

/**
 * /**
 * [State] to be rendered by [WebsitePermissionsView] displaying all explicitly allowed or blocked
 * website permissions.
 *
 * @param isVisible [Boolean] whether this contains data that needs to be displayed to the user.
 * @param camera [WebsitePermission] containing all information about the *camera* permission.
 * @param microphone [WebsitePermission] containing all information about the *microphone* permission.
 * @param notification [notification] containing all information about the *notification* permission.
 * @param location [WebsitePermission] containing all information about the *location* permission.
*/
 */
data class WebsitePermissionsState(
    val isVisible: Boolean,
    val camera: WebsitePermission,
    val microphone: WebsitePermission,
    val notification: WebsitePermission,
    val location: WebsitePermission,
    val autoplayAudible: WebsitePermission,
    val autoplayInaudible: WebsitePermission
) : State

/**
 * Wrapper over a website permission encompassing all it's needed state to be rendered on the screen.
 *
 * Contains a limited number of implementations because there is a known, finite number of permissions
 * we need to display to the user.
 */
sealed class WebsitePermission {
    /**
     * The *allowed* / *blocked* permission status to be shown to the user.
     */
    abstract val status: String

    /**
     * Whether this permission should be shown to the user.
     */
    abstract val isVisible: Boolean

    /**
     * Visual indication about whether this permission is *enabled* / *disabled*
     */
    abstract val isEnabled: Boolean

    /**
     * Whether the corresponding *dangerous* Android permission is granted for the app by the user or not.
     */
    abstract val isBlockedByAndroid: Boolean

    /**
     * Helper method mimicking the default generated *copy()* method for a data class.
     * Allows us using a familiar API in the reducer.
     */
    abstract fun copy(
        status: String = this.status,
        isVisible: Boolean = this.isVisible,
        isEnabled: Boolean = this.isEnabled,
        isBlockedByAndroid: Boolean = this.isBlockedByAndroid
    ): WebsitePermission

    /**
     * Contains all information about the *camera* permission.
     */
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

    /**
     * Contains all information about the *microphone* permission.
     */
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

    /**
     * Contains all information about the *notification* permission.
     */
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

    /**
     * Contains all information about the *location* permission.
     */
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

    /**
     * Contains all information about the *autoplay audible* permission.
     */
    data class AutoplayAudible(
        override val status: String,
        override val isVisible: Boolean,
        override val isEnabled: Boolean,
        override val isBlockedByAndroid: Boolean,
        val name: String = "AutoplayAudible" // helps to resolve the overload resolution ambiguity for the copy() method
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

    /**
     * Contains all information about the *autoplay inaudible* permission.
     */
    data class AutoplayInaudible(
        override val status: String,
        override val isVisible: Boolean,
        override val isEnabled: Boolean,
        override val isBlockedByAndroid: Boolean,
        // helps to resolve the overload resolution ambiguity for the copy() method
        val name: String = "AutoplayInaudible"
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

/**
 * Parent [Action] for all the [QuickSettingsFragmentState] changes.
 */
sealed class QuickSettingsFragmentAction : Action

/**
 * All possible [WebsiteInfoState] changes as result of user / system interactions.
 */
sealed class WebsiteInfoAction : QuickSettingsFragmentAction()

/**
 * All possible [WebsitePermissionsState] changes as result of user / system interactions.
 */
sealed class WebsitePermissionAction : QuickSettingsFragmentAction() {
    /**
     * Change resulting from toggling a specific [WebsitePermission] for the current website.
     *
     * @param updatedFeature [PhoneFeature] backing a certain [WebsitePermission].
     * Allows to easily identify which permission changed
     * **Must be the name of one of the properties of [WebsitePermissionsState]**.
     * @param updatedStatus [String] the new [WebsitePermission#status] which will be shown to the user.
     * @param updatedEnabledStatus [Boolean] the new [WebsitePermission#enabled] which will be shown to the user.
     */
    class TogglePermission(
        val websitePermission: WebsitePermission,
        val updatedStatus: String,
        val updatedEnabledStatus: Boolean
    ) : WebsitePermissionAction()
}

// -------------------------------------------------------------------------------------------------
// Reducers
// -------------------------------------------------------------------------------------------------

/**
 * Parent [Reducer] for all [QuickSettingsFragmentState]s of all Views shown in this Fragment.
 */
fun quickSettingsFragmentReducer(
    state: QuickSettingsFragmentState,
    action: QuickSettingsFragmentAction
): QuickSettingsFragmentState {
    return when (action) {
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

@Suppress("UNUSED_PARAMETER") // the action paramater is unused
object WebsiteInfoStateReducer {
    /**
     * Handles creating a new [WebsiteInfoState] based on the specific [WebsiteInfoAction]
     */
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
    /**
     * Handles creating a new [WebsitePermissionsState] based on the specific [WebsitePermissionAction]
     */
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
                    is WebsitePermission.AutoplayAudible -> {
                        return state.copy(
                            autoplayAudible = state.autoplayAudible.copy(
                                status = action.updatedStatus,
                                isEnabled = action.updatedEnabledStatus
                            )
                        )
                    }
                    is WebsitePermission.AutoplayInaudible -> {
                        return state.copy(
                            autoplayInaudible = state.autoplayInaudible.copy(
                                status = action.updatedStatus,
                                isEnabled = action.updatedEnabledStatus
                            )
                        )
                    }
                }
            }
        }
    }
}
