/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import mozilla.components.concept.engine.permission.Permission
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED

/**
 * Indicate how site permissions must behave by permission category.
 */
data class SitePermissionsRules(
    val camera: Action,
    val location: Action,
    val notification: Action,
    val microphone: Action
) {
    enum class Action {
        BLOCKED, ASK_TO_ALLOW;

        fun toStatus(): SitePermissions.Status = when (this) {
            BLOCKED -> SitePermissions.Status.BLOCKED
            ASK_TO_ALLOW -> SitePermissions.Status.NO_DECISION
        }
    }

    internal fun getActionFrom(request: PermissionRequest): Action {
        return if (request.containsVideoAndAudioSources()) {
            getActionForCombinedPermission()
        } else {
            getActionForSinglePermission(request.permissions.first())
        }
    }

    private fun getActionForSinglePermission(permission: Permission): Action {
        return when (permission) {
            is Permission.ContentGeoLocation -> {
                location
            }
            is Permission.ContentNotification -> {
                notification
            }
            is Permission.ContentAudioCapture, is Permission.ContentAudioMicrophone -> {
                microphone
            }
            is Permission.ContentVideoCamera, is Permission.ContentVideoCapture -> {
                camera
            }
            else -> ASK_TO_ALLOW
        }
    }

    private fun getActionForCombinedPermission(): Action {
        return if (camera == BLOCKED || microphone == BLOCKED) {
            BLOCKED
        } else {
            ASK_TO_ALLOW
        }
    }
}
