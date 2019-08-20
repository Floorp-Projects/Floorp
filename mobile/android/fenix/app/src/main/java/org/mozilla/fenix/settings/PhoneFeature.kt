/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.RECORD_AUDIO
import android.content.Context
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.mozilla.fenix.utils.Settings
import android.Manifest.permission.CAMERA as CAMERA_PERMISSION

const val ID_CAMERA_PERMISSION = 0
const val ID_LOCATION_PERMISSION = 1
const val ID_MICROPHONE_PERMISSION = 2
const val ID_NOTIFICATION_PERMISSION = 3

enum class PhoneFeature(val id: Int, val androidPermissionsList: Array<String>) {
    CAMERA(ID_CAMERA_PERMISSION, arrayOf(CAMERA_PERMISSION)),
    LOCATION(ID_LOCATION_PERMISSION, arrayOf(ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION)),
    MICROPHONE(ID_MICROPHONE_PERMISSION, arrayOf(RECORD_AUDIO)),
    NOTIFICATION(ID_NOTIFICATION_PERMISSION, emptyArray());

    fun isAndroidPermissionGranted(context: Context): Boolean {
        return when (this) {
            CAMERA, LOCATION, MICROPHONE -> context.isPermissionGranted(androidPermissionsList.asIterable())
            NOTIFICATION -> true
        }
    }

    fun getActionLabel(context: Context, sitePermissions: SitePermissions? = null, settings: Settings? = null): String {
        val label = when (this) {
            CAMERA -> {
                sitePermissions?.camera?.toString(context) ?: settings
                    ?.sitePermissionsPhoneFeatureCameraAction
                    ?.toString(context)
            }
            LOCATION -> {
                sitePermissions?.location?.toString(context) ?: settings
                    ?.sitePermissionsPhoneFeatureLocation
                    ?.toString(context)
            }
            MICROPHONE -> {
                sitePermissions?.microphone?.toString(context) ?: settings
                    ?.sitePermissionsPhoneFeatureMicrophoneAction
                    ?.toString(context)
            }
            NOTIFICATION -> {
                sitePermissions?.notification?.toString(context) ?: settings
                    ?.sitePermissionsPhoneFeatureNotificationAction
                    ?.toString(context)
            }
        }
        return requireNotNull(label)
    }

    fun getStatus(sitePermissions: SitePermissions? = null, settings: Settings? = null): SitePermissions.Status {
        val status = when (this) {
            CAMERA -> {
                sitePermissions?.camera ?: settings
                    ?.sitePermissionsPhoneFeatureCameraAction
                    ?.toStatus()
            }
            LOCATION -> {
                sitePermissions?.location ?: settings
                    ?.sitePermissionsPhoneFeatureLocation
                    ?.toStatus()
            }
            MICROPHONE -> {
                sitePermissions?.microphone ?: settings
                    ?.sitePermissionsPhoneFeatureMicrophoneAction
                    ?.toStatus()
            }
            NOTIFICATION -> {
                sitePermissions?.notification ?: settings
                    ?.sitePermissionsPhoneFeatureNotificationAction
                    ?.toStatus()
            }
        }
        return requireNotNull(status)
    }

    companion object {
        fun findFeatureBy(permissions: Array<out String>): PhoneFeature? {
            return PhoneFeature.values().find { feature ->
                feature.androidPermissionsList.any { permission ->
                    permission == permissions.first()
                }
            }
        }
    }
}
