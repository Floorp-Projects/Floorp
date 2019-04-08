/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.mozilla.fenix.R

internal fun SitePermissionsRules.Action.toString(context: Context): String {
    return when (this) {
        SitePermissionsRules.Action.ASK_TO_ALLOW -> {
            context.getString(R.string.preference_option_phone_feature_ask_to_allow)
        }
        SitePermissionsRules.Action.BLOCKED -> {
            context.getString(R.string.preference_option_phone_feature_blocked)
        }
    }
}

internal fun SitePermissions.Status.toString(context: Context): String {
    return when (this) {
        SitePermissions.Status.BLOCKED -> {
            context.getString(R.string.preference_option_phone_feature_blocked)
        }
        SitePermissions.Status.NO_DECISION -> {
            context.getString(R.string.preference_option_phone_feature_ask_to_allow)
        }
        SitePermissions.Status.ALLOWED -> {
            context.getString(R.string.preference_option_phone_feature_allowed)
        }
    }
}

fun SitePermissionsRules.Action.toStatus(): SitePermissions.Status {
    return when (this) {
        SitePermissionsRules.Action.BLOCKED -> SitePermissions.Status.BLOCKED
        SitePermissionsRules.Action.ASK_TO_ALLOW -> SitePermissions.Status.NO_DECISION
    }
}

fun SitePermissions.Status.toggle(): SitePermissions.Status {
    return when (this) {
        SitePermissions.Status.BLOCKED -> SitePermissions.Status.ALLOWED
        SitePermissions.Status.NO_DECISION -> SitePermissions.Status.ALLOWED
        SitePermissions.Status.ALLOWED -> SitePermissions.Status.BLOCKED
    }
}

fun SitePermissions.toggle(featurePhone: PhoneFeature): SitePermissions {
    return when (featurePhone) {
        PhoneFeature.CAMERA -> {
            copy(
                camera = camera.toggle()
            )
        }
        PhoneFeature.LOCATION -> {
            copy(
                location = location.toggle()
            )
        }
        PhoneFeature.MICROPHONE -> {
            copy(
                microphone = microphone.toggle()
            )
        }
        PhoneFeature.NOTIFICATION -> {
            copy(
                notification = notification.toggle()
            )
        }
    }
}
