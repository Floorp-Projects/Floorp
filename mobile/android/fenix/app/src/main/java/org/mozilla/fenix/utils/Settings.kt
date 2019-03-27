package org.mozilla.fenix.utils

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.content.SharedPreferences
import android.preference.PreferenceManager
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import java.security.InvalidParameterException

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 */
@SuppressWarnings("TooManyFunctions")
class Settings private constructor(context: Context) {

    companion object {
        const val autoBounceMaximumCount = 2

        var instance: Settings? = null

        @JvmStatic
        @Synchronized
        fun getInstance(context: Context): Settings {
            if (instance == null) {
                instance = Settings(context.applicationContext)
            }
            return instance ?: throw AssertionError("Instance cleared")
        }
    }

    private val appContext = context.applicationContext

    private val preferences: SharedPreferences =
        PreferenceManager.getDefaultSharedPreferences(context)

    val defaultSearchEngineName: String
        get() = preferences.getString(appContext.getPreferenceKey(R.string.pref_key_search_engine), "") ?: ""

    val isCrashReportingEnabled: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_crash_reporter), true) &&
                BuildConfig.CRASH_REPORTING && BuildConfig.BUILD_TYPE == "release"

    val isRemoteDebuggingEnabled: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_remote_debugging), false)

    val isTelemetryEnabled: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_telemetry), true)

    val shouldAutoBounceQuickActionSheet: Boolean
        get() = autoBounceQuickActionSheetCount < autoBounceMaximumCount

    val shouldRecommendedSettingsBeActivated: Boolean
        get() = preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_recommended_settings), true)

    val shouldUseLightTheme: Boolean
        get() = preferences.getBoolean(
            appContext.getPreferenceKey(R.string.pref_key_light_theme),
            false
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

    private val autoBounceQuickActionSheetCount: Int
        get() = (preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_bounce_quick_action), 0))

    fun incrementAutomaticBounceQuickActionSheetCount() {
        preferences.edit().putInt(appContext.getPreferenceKey(R.string.pref_key_bounce_quick_action),
            autoBounceQuickActionSheetCount + 1).apply()
    }

    fun setDefaultSearchEngineByName(name: String) {
        preferences.edit()
            .putString(appContext.getPreferenceKey(R.string.pref_key_search_engine), name)
            .apply()
    }

    fun showSearchSuggestions(): Boolean = preferences.getBoolean(
        appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions),
        true
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

    fun getSitePermissionsRecommendedSettingsRules() = SitePermissionsRules(
        camera = SitePermissionsRules.Action.ASK_TO_ALLOW,
        notification = SitePermissionsRules.Action.ASK_TO_ALLOW,
        location = SitePermissionsRules.Action.ASK_TO_ALLOW,
        microphone = SitePermissionsRules.Action.ASK_TO_ALLOW
    )

    fun getSitePermissionsCustomSettingsRules(): SitePermissionsRules {
        return SitePermissionsRules(
            notification = getSitePermissionsPhoneFeatureNotificationAction(),
            microphone = getSitePermissionsPhoneFeatureMicrophoneAction(),
            location = getSitePermissionsPhoneFeatureLocation(),
            camera = getSitePermissionsPhoneFeatureCameraAction()
        )
    }

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
