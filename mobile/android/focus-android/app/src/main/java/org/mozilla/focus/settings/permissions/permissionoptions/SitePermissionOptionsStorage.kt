/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions.permissionoptions

import android.content.Context
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.preference.PreferenceManager
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.mozilla.focus.R
import org.mozilla.focus.settings.permissions.AutoplayOption
import org.mozilla.focus.settings.permissions.SitePermissionOption

@Suppress("TooManyFunctions")
class SitePermissionOptionsStorage(private val context: Context) {

    /**
     * Return the label for Site Permission Option selected that will
     * appear in Site Permissions Screen
     */
    fun getSitePermissionOptionSelectedLabel(sitePermission: SitePermission): String {
        return if (isAndroidPermissionGranted(sitePermission)) {
            context.getString(permissionSelectedOption(sitePermission).titleId)
        } else {
            context.getString(R.string.phone_feature_blocked_by_android)
        }
    }

    fun isAndroidPermissionGranted(sitePermission: SitePermission): Boolean {
        return context.isPermissionGranted(sitePermission.androidPermissionsList.asIterable())
    }

    fun getSitePermissionLabel(sitePermission: SitePermission): String {
        return when (sitePermission) {
            SitePermission.CAMERA -> context.getString(R.string.preference_phone_feature_camera)
            SitePermission.LOCATION -> context.getString(R.string.preference_phone_feature_location)
            SitePermission.MICROPHONE -> context.getString(R.string.preference_phone_feature_microphone)
            SitePermission.NOTIFICATION -> context.getString(R.string.preference_phone_feature_notification)
            SitePermission.MEDIA_KEY_SYSTEM_ACCESS -> context.getString(
                R.string.preference_phone_feature_media_key_system_access,
            )
            SitePermission.AUTOPLAY, SitePermission.AUTOPLAY_AUDIBLE, SitePermission.AUTOPLAY_INAUDIBLE ->
                context.getString(R.string.preference_autoplay)
        }
    }

    /**
     * Return the available Options for a  Site Permission
     */
    fun getSitePermissionOptions(sitePermission: SitePermission): List<SitePermissionOption> {
        return when (sitePermission) {
            SitePermission.CAMERA -> listOf(
                SitePermissionOption.AskToAllow(),
                SitePermissionOption.Blocked(),
            )
            SitePermission.LOCATION -> listOf(
                SitePermissionOption.AskToAllow(),
                SitePermissionOption.Blocked(),
            )
            SitePermission.MICROPHONE -> listOf(
                SitePermissionOption.AskToAllow(),
                SitePermissionOption.Blocked(),
            )
            SitePermission.NOTIFICATION -> listOf(
                SitePermissionOption.AskToAllow(),
                SitePermissionOption.Blocked(),
            )
            SitePermission.MEDIA_KEY_SYSTEM_ACCESS -> listOf(
                SitePermissionOption.AskToAllow(),
                SitePermissionOption.Blocked(),
                SitePermissionOption.Allowed(),
            )
            SitePermission.AUTOPLAY, SitePermission.AUTOPLAY_AUDIBLE, SitePermission.AUTOPLAY_INAUDIBLE ->
                listOf(
                    AutoplayOption.AllowAudioVideo(),
                    AutoplayOption.BlockAudioOnly(),
                    AutoplayOption.BlockAudioVideo(),
                )
        }
    }

    /**
     * Return the default Option for a Site Permission if the user doesn't select nothing
     */
    @VisibleForTesting
    internal fun getSitePermissionDefaultOption(sitePermission: SitePermission): SitePermissionOption {
        return when (sitePermission) {
            SitePermission.CAMERA -> SitePermissionOption.AskToAllow()
            SitePermission.LOCATION -> SitePermissionOption.AskToAllow()
            SitePermission.MICROPHONE -> SitePermissionOption.AskToAllow()
            SitePermission.NOTIFICATION -> SitePermissionOption.AskToAllow()
            SitePermission.MEDIA_KEY_SYSTEM_ACCESS -> SitePermissionOption.AskToAllow()
            SitePermission.AUTOPLAY, SitePermission.AUTOPLAY_AUDIBLE, SitePermission.AUTOPLAY_INAUDIBLE ->
                AutoplayOption.BlockAudioOnly()
        }
    }

    /**
     * Return the user selected Option for a Site Permission or the default one if the user doesn't
     * select one
     */
    internal fun permissionSelectedOption(sitePermission: SitePermission) =
        when (permissionSelectedOptionByKey(getSitePermissionPreferenceId(sitePermission))) {
            context.getString(R.string.pref_key_allow_autoplay_audio_video) -> AutoplayOption.AllowAudioVideo()
            context.getString(R.string.pref_key_block_autoplay_audio_video) -> AutoplayOption.BlockAudioVideo()
            context.getString(R.string.pref_key_allowed) -> SitePermissionOption.Allowed()
            context.getString(R.string.pref_key_blocked) -> SitePermissionOption.Blocked()
            context.getString(R.string.pref_key_ask_to_allow) -> SitePermissionOption.AskToAllow()
            context.getString(R.string.pref_key_block_autoplay_audio_only) -> AutoplayOption.BlockAudioOnly()
            else -> {
                getSitePermissionDefaultOption(sitePermission)
            }
        }

    /**
     * Returns Site Permission corresponding resource ID from preference_keys
     */
    @StringRes
    fun getSitePermissionPreferenceId(sitePermission: SitePermission): Int {
        return when (sitePermission) {
            SitePermission.CAMERA -> R.string.pref_key_phone_feature_camera
            SitePermission.LOCATION -> R.string.pref_key_phone_feature_location
            SitePermission.MICROPHONE -> R.string.pref_key_phone_feature_microphone
            SitePermission.NOTIFICATION -> R.string.pref_key_phone_feature_notification
            SitePermission.AUTOPLAY -> R.string.pref_key_autoplay
            SitePermission.AUTOPLAY_AUDIBLE -> R.string.pref_key_allow_autoplay_audio_video
            SitePermission.AUTOPLAY_INAUDIBLE -> R.string.pref_key_block_autoplay_audio_video
            SitePermission.MEDIA_KEY_SYSTEM_ACCESS -> R.string.pref_key_browser_feature_media_key_system_access
        }
    }

    /**
     * Saves the current Site Permission Option
     *
     * @param sitePermissionOption to be Saved
     * @param sitePermission the corresponding Site Permission
     */
    fun saveCurrentSitePermissionOptionInSharePref(
        sitePermissionOption: SitePermissionOption,
        sitePermission: SitePermission,
    ) {
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
        with(sharedPref.edit()) {
            putString(
                context.getString(getSitePermissionPreferenceId(sitePermission)),
                context.getString(sitePermissionOption.prefKeyId),
            )
            apply()
        }
    }

    @VisibleForTesting
    internal fun permissionSelectedOptionByKey(
        sitePermissionKey: Int,
    ): String {
        val sharedPref = PreferenceManager.getDefaultSharedPreferences(context)
        return sharedPref.getString(context.getString(sitePermissionKey), "") ?: ""
    }

    private fun getAutoplayRules(): Pair<SitePermissionsRules.AutoplayAction, SitePermissionsRules.AutoplayAction> {
        return when (permissionSelectedOption(SitePermission.AUTOPLAY)) {
            is AutoplayOption.AllowAudioVideo -> Pair(
                SitePermissionsRules.AutoplayAction.ALLOWED,
                SitePermissionsRules.AutoplayAction.ALLOWED,
            )

            is AutoplayOption.BlockAudioVideo -> Pair(
                SitePermissionsRules.AutoplayAction.BLOCKED,
                SitePermissionsRules.AutoplayAction.BLOCKED,
            )

            else -> Pair(
                SitePermissionsRules.AutoplayAction.BLOCKED,
                SitePermissionsRules.AutoplayAction.ALLOWED,
            )
        }
    }

    fun getSitePermissionsSettingsRules() = SitePermissionsRules(
        notification = getSitePermissionRules(SitePermission.NOTIFICATION),
        microphone = getSitePermissionRules(SitePermission.MICROPHONE),
        location = getSitePermissionRules(SitePermission.LOCATION),
        camera = getSitePermissionRules(SitePermission.CAMERA),
        autoplayAudible = getAutoplayRules().first,
        autoplayInaudible = getAutoplayRules().second,
        persistentStorage = SitePermissionsRules.Action.BLOCKED,
        mediaKeySystemAccess = getSitePermissionRules(SitePermission.MEDIA_KEY_SYSTEM_ACCESS),
        crossOriginStorageAccess = SitePermissionsRules.Action.ASK_TO_ALLOW,
    )

    private fun getSitePermissionRules(sitePermission: SitePermission): SitePermissionsRules.Action {
        return when (permissionSelectedOption(sitePermission)) {
            is SitePermissionOption.Allowed -> SitePermissionsRules.Action.ALLOWED
            is SitePermissionOption.AskToAllow -> SitePermissionsRules.Action.ASK_TO_ALLOW
            is SitePermissionOption.Blocked -> SitePermissionsRules.Action.BLOCKED
            else -> {
                SitePermissionsRules.Action.BLOCKED
            }
        }
    }

    fun isSitePermissionNotBlocked(permissionsList: Array<String>): Boolean {
        SitePermission.entries.forEach { sitePermission ->
            if (
                sitePermission.androidPermissionsList.intersect(permissionsList.toSet()).isNotEmpty() &&
                getSitePermissionRules(sitePermission) != SitePermissionsRules.Action.BLOCKED
            ) {
                return true
            }
        }
        return false
    }
}
