/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.Context
import android.content.Context.MODE_PRIVATE
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import java.security.InvalidParameterException

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 */
@SuppressWarnings("TooManyFunctions")
class Settings private constructor(
    context: Context,
    private val isCrashReportEnabledInBuild: Boolean
) {

    companion object {
        const val autoBounceMaximumCount = 2
        const val FENIX_PREFERENCES = "fenix_preferences"

        var instance: Settings? = null

        @JvmStatic
        @Synchronized
        fun getInstance(
            context: Context,
            isCrashReportEnabledInBuild: Boolean = BuildConfig.CRASH_REPORTING && Config.channel.isReleased
        ): Settings {
            if (instance == null) {
                instance = Settings(context.applicationContext, isCrashReportEnabledInBuild)
            }
            return instance ?: throw AssertionError("Instance cleared")
        }
    }

    private val appContext = context.applicationContext

    val preferences: SharedPreferences =
        appContext.getSharedPreferences(FENIX_PREFERENCES, MODE_PRIVATE)

    val usePrivateMode: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_private_mode), false)

    fun setPrivateMode(newValue: Boolean) {
        preferences.edit().putBoolean(appContext.getPreferenceKey(R.string.pref_key_private_mode), newValue).apply()
    }

    val defaultSearchEngineName: String
        get() = preferences.getString(appContext.getPreferenceKey(R.string.pref_key_search_engine), "") ?: ""

    val isCrashReportingEnabled: Boolean
        get() = isCrashReportEnabledInBuild &&
                preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_crash_reporter), true)

    val isRemoteDebuggingEnabled: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_remote_debugging), false)

    val isTelemetryEnabled: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_telemetry), true)

    val shouldAutoBounceQuickActionSheet: Boolean
        get() = autoBounceQuickActionSheetCount < autoBounceMaximumCount

    val shouldUseLightTheme: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_light_theme),
            false
        )

    fun setLightTheme(newValue: Boolean) {
        preferences.edit().putBoolean(
            appContext.getPreferenceKey(R.string.pref_key_light_theme),
            newValue
        ).apply()
    }

    fun setAutoSize(newValue: Boolean) {
        preferences.edit().putBoolean(appContext.getPreferenceKey(R.string.pref_key_accessibility_auto_size), newValue)
            .apply()
    }

    val shouldUseAutoSize: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_accessibility_auto_size),
            true
        )

    fun setFontSizeFactor(newValue: Float) {
        preferences.edit().putFloat(appContext.getPreferenceKey(R.string.pref_key_accessibility_font_scale), newValue)
            .apply()
    }

    val fontSizeFactor: Float
        get() = preferences.getFloat(
            appContext.getPreferenceKey(R.string.pref_key_accessibility_font_scale),
            1f
        )

    val shouldShowVisitedSitesBookmarks: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_show_visited_sites_bookmarks),
            true
        )

    val shouldUseDarkTheme: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_dark_theme),
            false
        )

    val shouldFollowDeviceTheme: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_follow_device_theme),
            false
        )

    fun setFollowDeviceTheme(newValue: Boolean) {
        preferences.edit().putBoolean(
            appContext.getPreferenceKey(R.string.pref_key_follow_device_theme),
            newValue
        ).apply()
    }

    val shouldUseTrackingProtection: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_tracking_protection),
            true
        )

    fun setTrackingProtection(newValue: Boolean) {
        preferences.edit().putBoolean(
            appContext.getPreferenceKey(R.string.pref_key_tracking_protection),
            newValue
        ).apply()
    }

    val shouldUseAutoBatteryTheme: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_auto_battery_theme),
            false
        )

    val themeSettingString: String
        get() = when {
            shouldFollowDeviceTheme -> appContext.getString(R.string.preference_follow_device_theme)
            shouldUseAutoBatteryTheme -> appContext.getString(R.string.preference_auto_battery_theme)
            shouldUseDarkTheme -> appContext.getString(R.string.preference_dark_theme)
            shouldUseLightTheme -> appContext.getString(R.string.preference_light_theme)
            else -> appContext.getString(R.string.preference_light_theme)
        }

    @VisibleForTesting(otherwise = PRIVATE)
    internal val autoBounceQuickActionSheetCount: Int
        get() = (preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_bounce_quick_action), 0))

    fun incrementAutomaticBounceQuickActionSheetCount() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_bounce_quick_action),
            autoBounceQuickActionSheetCount + 1
        ).apply()
    }

    fun setDefaultSearchEngineByName(name: String) {
        preferences.edit()
            .putString(appContext.getPreferenceKey(R.string.pref_key_search_engine), name)
            .apply()
    }

    val showSearchSuggestions: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions), true
        )

    fun setSitePermissionsPhoneFeatureCameraAction(action: SitePermissionsRules.Action) {
        preferences.edit()
            .putInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_camera), action.id)
            .apply()
    }

    fun getSitePermissionsPhoneFeatureCameraAction(): SitePermissionsRules.Action {
        return preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_camera), 1)
            .toSitePermissionsRulesAction()
    }

    fun setSitePermissionsPhoneFeatureMicrophoneAction(action: SitePermissionsRules.Action) {
        preferences.edit()
            .putInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_microphone), action.id)
            .apply()
    }

    fun getSitePermissionsPhoneFeatureMicrophoneAction(): SitePermissionsRules.Action {
        return preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_microphone), 1)
            .toSitePermissionsRulesAction()
    }

    fun setSitePermissionsPhoneFeatureNotificationAction(action: SitePermissionsRules.Action) {
        preferences.edit()
            .putInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_notification), action.id)
            .apply()
    }

    fun getSitePermissionsPhoneFeatureNotificationAction(): SitePermissionsRules.Action {
        return preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_notification), 1)
            .toSitePermissionsRulesAction()
    }

    fun setSitePermissionsPhoneFeatureLocation(action: SitePermissionsRules.Action) {
        preferences.edit()
            .putInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_location), action.id)
            .apply()
    }

    fun getSitePermissionsPhoneFeatureLocation(): SitePermissionsRules.Action {
        return preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_phone_feature_location), 1)
            .toSitePermissionsRulesAction()
    }

    fun getSitePermissionsCustomSettingsRules(): SitePermissionsRules {
        return SitePermissionsRules(
            notification = getSitePermissionsPhoneFeatureNotificationAction(),
            microphone = getSitePermissionsPhoneFeatureMicrophoneAction(),
            location = getSitePermissionsPhoneFeatureLocation(),
            camera = getSitePermissionsPhoneFeatureCameraAction()
        )
    }

    fun setFxaSignedIn(isSignedIn: Boolean) {
        preferences.edit()
            .putBoolean(appContext.getPreferenceKey(R.string.pref_key_fxa_signed_in), isSignedIn)
            .apply()
    }

    val fxaSignedIn: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_fxa_signed_in), true
        )

    fun setFxaHasSyncedItems(hasSyncedItems: Boolean) {
        preferences.edit()
            .putBoolean(appContext.getPreferenceKey(R.string.pref_key_fxa_has_synced_items), hasSyncedItems)
            .apply()
    }

    val fxaHasSyncedItems: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_fxa_has_synced_items), true
        )

    private val SitePermissionsRules.Action.id: Int
        get() {
            return when (this) {
                SitePermissionsRules.Action.BLOCKED -> 0
                SitePermissionsRules.Action.ASK_TO_ALLOW -> 1
            }
        }

    private fun Int.toSitePermissionsRulesAction(): SitePermissionsRules.Action {
        return when (this) {
            0 -> SitePermissionsRules.Action.BLOCKED
            1 -> SitePermissionsRules.Action.ASK_TO_ALLOW
            else -> throw InvalidParameterException("$this is not a valid SitePermissionsRules.Action")
        }
    }
}
