/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.RECORD_AUDIO
import android.content.Context
import android.os.Parcelable
import androidx.annotation.StringRes
import kotlinx.android.parcel.Parcelize
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_ALLOW_ALL
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_ALLOW_ON_WIFI
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_BLOCK_ALL
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_BLOCK_AUDIBLE
import org.mozilla.fenix.utils.Settings
import android.Manifest.permission.CAMERA as CAMERA_PERMISSION

@Parcelize
enum class PhoneFeature(val androidPermissionsList: Array<String>) : Parcelable {
    CAMERA(arrayOf(CAMERA_PERMISSION)),
    LOCATION(arrayOf(ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION)),
    MICROPHONE(arrayOf(RECORD_AUDIO)),
    NOTIFICATION(emptyArray()),
    AUTOPLAY_AUDIBLE(emptyArray()),
    AUTOPLAY_INAUDIBLE(emptyArray());

    fun isAndroidPermissionGranted(context: Context): Boolean {
        return context.isPermissionGranted(androidPermissionsList.asIterable())
    }

    @Suppress("ComplexMethod")
    fun getActionLabel(
        context: Context,
        sitePermissions: SitePermissions? = null,
        settings: Settings? = null
    ): String {
        @StringRes val stringRes =
            when (isAndroidPermissionGranted(context)) {
                false -> R.string.phone_feature_blocked_by_android
                else -> when (this) {
                    AUTOPLAY_AUDIBLE -> {
                        when (settings?.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) ?: AUTOPLAY_BLOCK_ALL) {
                            AUTOPLAY_ALLOW_ALL -> R.string.preference_option_autoplay_allowed2
                            AUTOPLAY_ALLOW_ON_WIFI -> R.string.preference_option_autoplay_allowed_wifi_only2
                            AUTOPLAY_BLOCK_AUDIBLE -> R.string.preference_option_autoplay_block_audio2
                            AUTOPLAY_BLOCK_ALL -> R.string.preference_option_autoplay_blocked3
                            else -> R.string.preference_option_autoplay_blocked3
                        }
                    }
                    else -> {
                        when (getStatus(sitePermissions, settings)) {
                            SitePermissions.Status.BLOCKED -> R.string.preference_option_phone_feature_blocked
                            SitePermissions.Status.NO_DECISION -> R.string.preference_option_phone_feature_ask_to_allow
                            SitePermissions.Status.ALLOWED -> R.string.preference_option_phone_feature_allowed
                        }
                    }
                }
            }
        return context.getString(stringRes)
    }

    fun getStatus(
        sitePermissions: SitePermissions? = null,
        settings: Settings? = null
    ): SitePermissions.Status {
        val status = sitePermissions?.get(this) ?: settings?.let(::getAction)?.toStatus()
        return requireNotNull(status)
    }

    fun getLabel(context: Context): String {
        return when (this) {
            CAMERA -> context.getString(R.string.preference_phone_feature_camera)
            LOCATION -> context.getString(R.string.preference_phone_feature_location)
            MICROPHONE -> context.getString(R.string.preference_phone_feature_microphone)
            NOTIFICATION -> context.getString(R.string.preference_phone_feature_notification)
            AUTOPLAY_AUDIBLE, AUTOPLAY_INAUDIBLE -> context.getString(R.string.preference_browser_feature_autoplay)
        }
    }

    fun getPreferenceKey(context: Context): String {
        return when (this) {
            CAMERA -> context.getPreferenceKey(R.string.pref_key_phone_feature_camera)
            LOCATION -> context.getPreferenceKey(R.string.pref_key_phone_feature_location)
            MICROPHONE -> context.getPreferenceKey(R.string.pref_key_phone_feature_microphone)
            NOTIFICATION -> context.getPreferenceKey(R.string.pref_key_phone_feature_notification)
            AUTOPLAY_AUDIBLE -> context.getPreferenceKey(R.string.pref_key_browser_feature_autoplay_audible)
            AUTOPLAY_INAUDIBLE -> context.getPreferenceKey(R.string.pref_key_browser_feature_autoplay_inaudible)
        }
    }

    fun getAction(settings: Settings): SitePermissionsRules.Action =
        settings.getSitePermissionsPhoneFeatureAction(this, getDefault())

    fun getDefault(): SitePermissionsRules.Action {
        return when (this) {
            AUTOPLAY_AUDIBLE -> SitePermissionsRules.Action.BLOCKED
            AUTOPLAY_INAUDIBLE -> SitePermissionsRules.Action.ALLOWED
            else -> SitePermissionsRules.Action.ASK_TO_ALLOW
        }
    }

    companion object {
        fun findFeatureBy(permissions: Array<out String>): PhoneFeature? {
            return values().find { feature ->
                feature.androidPermissionsList.any { permission ->
                    permission == permissions.first()
                }
            }
        }
    }
}
