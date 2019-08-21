/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.RECORD_AUDIO
import android.content.Context
import androidx.annotation.StringRes
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
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
        @StringRes val stringRes = when (getStatus(sitePermissions, settings)) {
            SitePermissions.Status.BLOCKED -> R.string.preference_option_phone_feature_blocked
            SitePermissions.Status.NO_DECISION -> R.string.preference_option_phone_feature_ask_to_allow
            SitePermissions.Status.ALLOWED -> R.string.preference_option_phone_feature_allowed
        }
        return context.getString(stringRes)
    }

    fun getStatus(sitePermissions: SitePermissions? = null, settings: Settings? = null): SitePermissions.Status {
        val status = getStatus(sitePermissions) ?: settings?.let(::getAction)?.toStatus()
        return requireNotNull(status)
    }

    fun getLabel(context: Context): String {
        return when (this) {
            CAMERA -> context.getString(R.string.preference_phone_feature_camera)
            LOCATION -> context.getString(R.string.preference_phone_feature_location)
            MICROPHONE -> context.getString(R.string.preference_phone_feature_microphone)
            NOTIFICATION -> context.getString(R.string.preference_phone_feature_notification)
        }
    }

    fun getPreferenceKey(context: Context): String {
        return when (this) {
            CAMERA -> context.getPreferenceKey(R.string.pref_key_phone_feature_camera)
            LOCATION -> context.getPreferenceKey(R.string.pref_key_phone_feature_location)
            MICROPHONE -> context.getPreferenceKey(R.string.pref_key_phone_feature_microphone)
            NOTIFICATION -> context.getPreferenceKey(R.string.pref_key_phone_feature_notification)
        }
    }

    fun getAction(settings: Settings): SitePermissionsRules.Action =
        settings.getSitePermissionsPhoneFeatureAction(this)

    private fun getStatus(sitePermissions: SitePermissions?): SitePermissions.Status? {
        sitePermissions ?: return null
        return when (this) {
            CAMERA -> sitePermissions.camera
            LOCATION -> sitePermissions.location
            MICROPHONE -> sitePermissions.microphone
            NOTIFICATION -> sitePermissions.notification
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
