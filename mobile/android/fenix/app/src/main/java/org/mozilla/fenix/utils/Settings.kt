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
import org.mozilla.fenix.settings.sharedpreferences.PreferencesHolder
import org.mozilla.fenix.settings.sharedpreferences.booleanPreference
import org.mozilla.fenix.settings.sharedpreferences.sitePermissionsRulesActionPreference

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 */
class Settings private constructor(
    context: Context,
    private val isCrashReportEnabledInBuild: Boolean
) : PreferencesHolder {

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

    override val preferences: SharedPreferences =
        appContext.getSharedPreferences(FENIX_PREFERENCES, MODE_PRIVATE)

    var usePrivateMode by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_private_mode),
        default = false
    )

    var defaultSearchEngineName: String
        get() = preferences.getString(appContext.getPreferenceKey(R.string.pref_key_search_engine), "") ?: ""
        set(name) = preferences.edit()
            .putString(appContext.getPreferenceKey(R.string.pref_key_search_engine), name)
            .apply()

    val isCrashReportingEnabled: Boolean
        get() = isCrashReportEnabledInBuild &&
                preferences.getBoolean(appContext.getPreferenceKey(R.string.pref_key_crash_reporter), true)

    val isRemoteDebuggingEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_remote_debugging),
        default = false
    )

    val isTelemetryEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_telemetry),
        default = true
    )

    val shouldAutoBounceQuickActionSheet: Boolean
        get() = autoBounceQuickActionSheetCount < autoBounceMaximumCount

    var shouldUseLightTheme by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_light_theme),
        default = false
    )

    var shouldUseAutoSize by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_accessibility_auto_size),
        default = true
    )

    var fontSizeFactor: Float
        get() = preferences.getFloat(
            appContext.getPreferenceKey(R.string.pref_key_accessibility_font_scale),
            1f
        )
        set(value) = preferences.edit()
            .putFloat(appContext.getPreferenceKey(R.string.pref_key_accessibility_font_scale), value)
            .apply()

    val shouldShowVisitedSitesBookmarks by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_visited_sites_bookmarks),
        default = true
    )

    val shouldUseDarkTheme by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_dark_theme),
        default = false
    )

    var shouldFollowDeviceTheme by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_follow_device_theme),
        default = false
    )

    var shouldUseTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection),
        default = true
    )

    val shouldUseAutoBatteryTheme by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_auto_battery_theme),
        default = false
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
        get() = preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_bounce_quick_action), 0)

    fun incrementAutomaticBounceQuickActionSheetCount() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_bounce_quick_action),
            autoBounceQuickActionSheetCount + 1
        ).apply()
    }

    val showSearchSuggestions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions),
        default = true
    )

    var sitePermissionsPhoneFeatureCameraAction by sitePermissionsRulesActionPreference(
        appContext.getPreferenceKey(R.string.pref_key_phone_feature_camera)
    )

    var sitePermissionsPhoneFeatureMicrophoneAction by sitePermissionsRulesActionPreference(
        appContext.getPreferenceKey(R.string.pref_key_phone_feature_microphone)
    )

    var sitePermissionsPhoneFeatureNotificationAction by sitePermissionsRulesActionPreference(
        appContext.getPreferenceKey(R.string.pref_key_phone_feature_notification)
    )

    var sitePermissionsPhoneFeatureLocation by sitePermissionsRulesActionPreference(
        appContext.getPreferenceKey(R.string.pref_key_phone_feature_location)
    )

    fun getSitePermissionsCustomSettingsRules(): SitePermissionsRules {
        return SitePermissionsRules(
            notification = sitePermissionsPhoneFeatureNotificationAction,
            microphone = sitePermissionsPhoneFeatureMicrophoneAction,
            location = sitePermissionsPhoneFeatureLocation,
            camera = sitePermissionsPhoneFeatureCameraAction
        )
    }

    var fxaSignedIn by booleanPreference(appContext.getPreferenceKey(R.string.pref_key_fxa_signed_in), default = true)

    var fxaHasSyncedItems by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_fxa_has_synced_items),
        default = true
    )

    fun addSearchWidgetInstalled(count: Int) {
        val key = appContext.getPreferenceKey(R.string.pref_key_search_widget_installed)
        val newValue = preferences.getInt(key, 0) + count
        preferences.edit()
            .putInt(key, newValue)
            .apply()
    }

    val searchWidgetInstalled: Boolean
        get() = 0 < preferences.getInt(appContext.getPreferenceKey(R.string.pref_key_search_widget_installed), 0)
}
