/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import mozilla.components.concept.engine.permission.Permission
import mozilla.components.support.base.Component.FEATURE_SITEPERMISSIONS
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the site permissions prompt.
 */
class SitePermissionsFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val PERMISSIONS = "permissions"
    }
}

internal fun emitPermissionDialogDisplayed(permission: Permission) = emitSitePermissionsFact(
    action = Action.DISPLAY,
    permissions = permission.name,
)

internal fun emitPermissionsDialogDisplayed(permissions: List<Permission>) = emitSitePermissionsFact(
    action = Action.DISPLAY,
    permissions = permissions.distinctBy { it.name }.joinToString { it.name },
)

internal fun emitPermissionDenied(permission: Permission) = emitSitePermissionsFact(
    action = Action.CANCEL,
    permissions = permission.name,
)

internal fun emitPermissionsDenied(permissions: List<Permission>) = emitSitePermissionsFact(
    action = Action.CANCEL,
    permissions = permissions.distinctBy { it.name }.joinToString { it.name },
)

internal fun emitPermissionAllowed(permission: Permission) = emitSitePermissionsFact(
    action = Action.CONFIRM,
    permissions = permission.name,
)

internal fun emitPermissionsAllowed(permissions: List<Permission>) = emitSitePermissionsFact(
    action = Action.CONFIRM,
    permissions = permissions.distinctBy { it.name }.joinToString { it.name },
)

private fun emitSitePermissionsFact(
    action: Action,
    permissions: String,
) {
    Fact(
        FEATURE_SITEPERMISSIONS,
        action,
        SitePermissionsFacts.Items.PERMISSIONS,
        permissions,
    ).collect()
}
