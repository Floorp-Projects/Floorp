/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.utils.Settings

class WebsitePermissionsStore(
    initialState: WebsitePermissionsState
) : Store<WebsitePermissionsState, WebsitePermissionAction>(
    initialState, ::reducer
) {
    companion object {
        fun createStore(
            context: Context,
            permissions: SitePermissions?,
            settings: Settings
        ) = WebsitePermissionsStore(
            WebsitePermissionsState(
                camera = initWebsitePermission(context, PhoneFeature.CAMERA, permissions, settings),
                microphone = initWebsitePermission(context, PhoneFeature.MICROPHONE, permissions, settings),
                notification = initWebsitePermission(context, PhoneFeature.NOTIFICATION, permissions, settings),
                location = initWebsitePermission(context, PhoneFeature.LOCATION, permissions, settings)
            )
        )

        private fun initWebsitePermission(
            context: Context,
            phoneFeature: PhoneFeature,
            permissions: SitePermissions?,
            settings: Settings
        ): WebsitePermission {
            val shouldBeVisible = phoneFeature.shouldBeVisible(permissions, settings)

            return WebsitePermission(
                name = phoneFeature.name,
                status = phoneFeature.getActionLabel(context, permissions, settings),
                visible = shouldBeVisible,
                enabled = shouldBeVisible &&
                    phoneFeature.isAndroidPermissionGranted(context) &&
                    !phoneFeature.isUserPermissionGranted(permissions, settings)
            )
        }

        private fun PhoneFeature.shouldBeVisible(
            sitePermissions: SitePermissions?,
            settings: Settings
        ) = getStatus(sitePermissions, settings) != SitePermissions.Status.NO_DECISION

        private fun PhoneFeature.isUserPermissionGranted(
            sitePermissions: SitePermissions?,
            settings: Settings
        ) = getStatus(sitePermissions, settings) == SitePermissions.Status.BLOCKED
    }
}

data class WebsitePermissionsState(
    val camera: WebsitePermission,
    val microphone: WebsitePermission,
    val notification: WebsitePermission,
    val location: WebsitePermission
) : State

sealed class WebsitePermissionAction : Action {
    object Stub1 : WebsitePermissionAction()
    object Stub2 : WebsitePermissionAction()
}

data class WebsitePermission(
    val name: String,
    val status: String,
    val visible: Boolean,
    val enabled: Boolean
)

fun reducer(
    state: WebsitePermissionsState,
    action: WebsitePermissionAction
): WebsitePermissionsState {
    return when (action) {
        WebsitePermissionAction.Stub1 -> state
        WebsitePermissionAction.Stub2 -> state
    }
}
