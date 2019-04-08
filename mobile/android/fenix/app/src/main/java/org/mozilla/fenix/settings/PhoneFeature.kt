/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.Manifest
import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.RECORD_AUDIO
import android.content.Context
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.mozilla.fenix.utils.Settings

enum class PhoneFeature(val id: Int, val androidPermissionsList: Array<String>) {
    CAMERA(SitePermissionsManagePhoneFeature.CAMERA_PERMISSION, arrayOf(Manifest.permission.CAMERA)),
    LOCATION(
        SitePermissionsManagePhoneFeature.LOCATION_PERMISSION, arrayOf(
            ACCESS_COARSE_LOCATION,
            ACCESS_FINE_LOCATION
        )
    ),
    MICROPHONE(SitePermissionsManagePhoneFeature.MICROPHONE_PERMISSION, arrayOf(RECORD_AUDIO)),
    NOTIFICATION(SitePermissionsManagePhoneFeature.NOTIFICATION_PERMISSION, emptyArray());

    @Suppress("SpreadOperator")
    fun isAndroidPermissionGranted(context: Context): Boolean {
        val permissions = when (this) {
            CAMERA, LOCATION, MICROPHONE -> androidPermissionsList
            NOTIFICATION -> return true
        }
        return context.isPermissionGranted(*permissions)
    }

    fun getActionLabel(context: Context, sitePermissions: SitePermissions? = null, settings: Settings): String {
        return when (this) {
            CAMERA -> {
                sitePermissions?.camera?.toString(context) ?: settings
                    .getSitePermissionsPhoneFeatureCameraAction()
                    .toString(context)
            }
            LOCATION -> {
                sitePermissions?.location?.toString(context) ?: settings
                    .getSitePermissionsPhoneFeatureLocation()
                    .toString(context)
            }
            MICROPHONE -> {
                sitePermissions?.microphone?.toString(context) ?: settings
                    .getSitePermissionsPhoneFeatureMicrophoneAction()
                    .toString(context)
            }
            NOTIFICATION -> {
                sitePermissions?.notification?.toString(context) ?: settings
                    .getSitePermissionsPhoneFeatureNotificationAction()
                    .toString(context)
            }
        }
    }

    fun getStatus(sitePermissions: SitePermissions? = null, settings: Settings): SitePermissions.Status {
        return when (this) {
            CAMERA -> {
                sitePermissions?.camera ?: settings
                    .getSitePermissionsPhoneFeatureCameraAction()
                    .toStatus()
            }
            LOCATION -> {
                sitePermissions?.location ?: settings
                    .getSitePermissionsPhoneFeatureLocation()
                    .toStatus()
            }
            MICROPHONE -> {
                sitePermissions?.microphone ?: settings
                    .getSitePermissionsPhoneFeatureMicrophoneAction()
                    .toStatus()
            }
            NOTIFICATION -> {
                sitePermissions?.notification ?: settings
                    .getSitePermissionsPhoneFeatureNotificationAction()
                    .toStatus()
            }
        }
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
