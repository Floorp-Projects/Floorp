/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.accessibilityservice.AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES
import android.app.Application
import android.content.Context
import android.content.Context.MODE_PRIVATE
import android.content.SharedPreferences
import android.content.pm.ShortcutManager
import android.os.Build
import android.view.accessibility.AccessibilityManager
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.lifecycle.LifecycleOwner
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action
import mozilla.components.feature.sitepermissions.SitePermissionsRules.AutoplayAction
import mozilla.components.support.ktx.android.content.PreferencesHolder
import mozilla.components.support.ktx.android.content.booleanPreference
import mozilla.components.support.ktx.android.content.floatPreference
import mozilla.components.support.ktx.android.content.intPreference
import mozilla.components.support.ktx.android.content.longPreference
import mozilla.components.support.ktx.android.content.stringPreference
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.MozillaProductDetector
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType
import org.mozilla.fenix.settings.logins.SavedLoginsSortingStrategyMenu
import org.mozilla.fenix.settings.logins.SortingStrategy
import org.mozilla.fenix.settings.registerOnSharedPreferenceChangeListener
import java.security.InvalidParameterException

private const val AUTOPLAY_USER_SETTING = "AUTOPLAY_USER_SETTING"

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 * @param appContext Reference to application context.
 */
@Suppress("LargeClass", "TooManyFunctions")
class Settings(private val appContext: Context) : PreferencesHolder {

    companion object {
        const val showLoginsSecureWarningSyncMaxCount = 1
        const val showLoginsSecureWarningMaxCount = 1
        const val trackingProtectionOnboardingMaximumCount = 1
        const val pwaVisitsToShowPromptMaxCount = 3
        const val FENIX_PREFERENCES = "fenix_preferences"

        private const val showSearchWidgetCFRMaxCount = 3
        private const val BLOCKED_INT = 0
        private const val ASK_TO_ALLOW_INT = 1
        private const val ALLOWED_INT = 2
        private const val CFR_COUNT_CONDITION_FOCUS_INSTALLED = 1
        private const val CFR_COUNT_CONDITION_FOCUS_NOT_INSTALLED = 3

        private fun Action.toInt() = when (this) {
            Action.BLOCKED -> BLOCKED_INT
            Action.ASK_TO_ALLOW -> ASK_TO_ALLOW_INT
            Action.ALLOWED -> ALLOWED_INT
        }

        private fun AutoplayAction.toInt() = when (this) {
            AutoplayAction.BLOCKED -> BLOCKED_INT
            AutoplayAction.ALLOWED -> ALLOWED_INT
        }

        private fun Int.toAction() = when (this) {
            BLOCKED_INT -> Action.BLOCKED
            ASK_TO_ALLOW_INT -> Action.ASK_TO_ALLOW
            ALLOWED_INT -> Action.ALLOWED
            else -> throw InvalidParameterException("$this is not a valid SitePermissionsRules.Action")
        }

        private fun Int.toAutoplayAction() = when (this) {
            BLOCKED_INT -> AutoplayAction.BLOCKED
            ALLOWED_INT -> AutoplayAction.ALLOWED
            // Users from older versions may have saved invalid values. Migrate them to BLOCKED
            ASK_TO_ALLOW_INT -> AutoplayAction.BLOCKED
            else -> throw InvalidParameterException("$this is not a valid SitePermissionsRules.AutoplayAction")
        }
    }

    @VisibleForTesting
    internal val isCrashReportEnabledInBuild: Boolean =
        BuildConfig.CRASH_REPORTING && Config.channel.isReleased

    override val preferences: SharedPreferences =
        appContext.getSharedPreferences(FENIX_PREFERENCES, MODE_PRIVATE)

    var forceEnableZoom by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_accessibility_force_enable_zoom),
        default = false
    )

    var adjustCampaignId by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_adjust_campaign),
        default = ""
    )

    var adjustNetwork by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_adjust_network),
        default = ""
    )

    var adjustAdGroup by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_adjust_adgroup),
        default = ""
    )

    var adjustCreative by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_adjust_creative),
        default = ""
    )

    var openLinksInAPrivateTab by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_open_links_in_a_private_tab),
        default = false
    )

    var allowScreenshotsInPrivateMode by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_allow_screenshots_in_private_mode),
        default = false
    )

    // If any of the prefs have been modified, quit displaying the fenix moved tip
    fun shouldDisplayFenixMovingTip(): Boolean =
        preferences.getBoolean(
            appContext.getString(R.string.pref_key_migrating_from_fenix_nightly_tip),
            true
        ) &&
                preferences.getBoolean(
                    appContext.getString(R.string.pref_key_migrating_from_firefox_nightly_tip),
                    true
                ) &&
                preferences.getBoolean(
                    appContext.getString(R.string.pref_key_migrating_from_fenix_tip),
                    true
                )

    private val activeSearchCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_count),
        default = 0
    )

    fun incrementActiveSearchCount() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_search_count),
            activeSearchCount + 1
        ).apply()
    }

    private val isActiveSearcher: Boolean
        get() = activeSearchCount > 2

    fun shouldDisplaySearchWidgetCFR(): Boolean =
        isActiveSearcher &&
                searchWidgetCFRDismissCount < showSearchWidgetCFRMaxCount &&
                !searchWidgetInstalled &&
                !searchWidgetCFRManuallyDismissed

    private val searchWidgetCFRDisplayCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_widget_cfr_display_count),
        default = 0
    )

    fun incrementSearchWidgetCFRDisplayed() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_search_widget_cfr_display_count),
            searchWidgetCFRDisplayCount + 1
        ).apply()
    }

    private val searchWidgetCFRManuallyDismissed by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_widget_cfr_manually_dismissed),
        default = false
    )

    fun manuallyDismissSearchWidgetCFR() {
        preferences.edit().putBoolean(
            appContext.getPreferenceKey(R.string.pref_key_search_widget_cfr_manually_dismissed),
            true
        ).apply()
    }

    private val searchWidgetCFRDismissCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_widget_cfr_dismiss_count),
        default = 0
    )

    fun incrementSearchWidgetCFRDismissed() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_search_widget_cfr_dismiss_count),
            searchWidgetCFRDismissCount + 1
        ).apply()
    }

    val isInSearchWidgetExperiment by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_is_in_search_widget_experiment),
        default = false
    )

    fun setSearchWidgetExperiment(value: Boolean) {
        preferences.edit().putBoolean(
            appContext.getPreferenceKey(R.string.pref_key_is_in_search_widget_experiment),
            value
        ).apply()
    }

    var defaultSearchEngineName by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_engine),
        default = ""
    )

    var openInAppOpened by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_open_in_app_opened),
        default = false
    )

    var installPwaOpened by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_install_pwa_opened),
        default = false
    )

    val isCrashReportingEnabled: Boolean
        get() = isCrashReportEnabledInBuild &&
                preferences.getBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_crash_reporter),
                    true
                )

    val isRemoteDebuggingEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_remote_debugging),
        default = false
    )

    val isTelemetryEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_telemetry),
        default = true
    )

    val isMarketingTelemetryEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_marketing_telemetry),
        default = true
    )

    val isExperimentationEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_experimentation),
        default = true
    )

    private var trackingProtectionOnboardingShownThisSession = false
    var isOverrideTPPopupsForPerformanceTest = false

    val shouldShowTrackingProtectionOnboarding: Boolean
        get() = !isOverrideTPPopupsForPerformanceTest &&
                (trackingProtectionOnboardingCount < trackingProtectionOnboardingMaximumCount &&
                        !trackingProtectionOnboardingShownThisSession)

    var showSecretDebugMenuThisSession = false

    val shouldShowSecurityPinWarningSync: Boolean
        get() = loginsSecureWarningSyncCount < showLoginsSecureWarningSyncMaxCount

    val shouldShowSecurityPinWarning: Boolean
        get() = loginsSecureWarningCount < showLoginsSecureWarningMaxCount

    var shouldUseLightTheme by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_light_theme),
        default = false
    )

    var shouldUseAutoSize by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_accessibility_auto_size),
        default = true
    )

    var fontSizeFactor by floatPreference(
        appContext.getPreferenceKey(R.string.pref_key_accessibility_font_scale),
        default = 1f
    )

    val shouldShowHistorySuggestions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_browsing_history),
        default = true
    )

    val shouldShowBookmarkSuggestions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_bookmarks),
        default = true
    )

    val shouldShowClipboardSuggestions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_clipboard_suggestions),
        default = true
    )

    val shouldShowSearchShortcuts by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_search_engine_shortcuts),
        default = false
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

    /**
     * Caches the last known "is default browser" state when the app was paused.
     * For an up to do date state use `isDefaultBrowser` instead.
     */
    var wasDefaultBrowserOnLastPause by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_default_browser),
        default = isDefaultBrowser()
    )

    fun isDefaultBrowser(): Boolean {
        val browsers = BrowsersCache.all(appContext)
        return browsers.isDefaultBrowser
    }

    val shouldUseAutoBatteryTheme by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_auto_battery_theme),
        default = false
    )

    val useStandardTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_standard_option),
        true
    )

    val useStrictTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_strict_default),
        false
    )

    val useCustomTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_option),
        false
    )

    @VisibleForTesting(otherwise = PRIVATE)
    fun setStrictETP() {
        preferences.edit().putBoolean(
            appContext.getPreferenceKey(R.string.pref_key_tracking_protection_strict_default),
            true
        ).apply()
        preferences.edit().putBoolean(
            appContext.getPreferenceKey(R.string.pref_key_tracking_protection_standard_option),
            false
        ).apply()
        appContext.components.let {
            val policy = it.core.trackingProtectionPolicyFactory
                .createTrackingProtectionPolicy()
            it.useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
            it.useCases.sessionUseCases.reload.invoke()
        }
    }

    val blockCookiesInCustomTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_cookies),
        true
    )

    val blockCookiesSelectionInCustomTrackingProtection by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_cookies_select),
        "social"
    )

    val blockTrackingContentInCustomTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_tracking_content),
        true
    )

    val blockTrackingContentSelectionInCustomTrackingProtection by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_tracking_content_select),
        "all"
    )

    val blockCryptominersInCustomTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_cryptominers),
        true
    )

    val blockFingerprintersInCustomTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_fingerprinters),
        true
    )

    val shouldUseFixedTopToolbar: Boolean
        get() {
            return touchExplorationIsEnabled || switchServiceIsEnabled
        }

    var lastKnownMode: BrowsingMode = BrowsingMode.Normal
        get() {
            val lastKnownModeWasPrivate = preferences.getBoolean(
                appContext.getPreferenceKey(R.string.pref_key_last_known_mode_private),
                false
            )

            return if (lastKnownModeWasPrivate) {
                BrowsingMode.Private
            } else {
                BrowsingMode.Normal
            }
        }
        set(value) {
            val lastKnownModeWasPrivate = (value == BrowsingMode.Private)

            preferences.edit()
                .putBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_last_known_mode_private),
                    lastKnownModeWasPrivate
                )
                .apply()

            field = value
        }

    var shouldDeleteBrowsingDataOnQuit by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_delete_browsing_data_on_quit),
        default = false
    )

    var deleteOpenTabs by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_delete_open_tabs_now),
        default = true
    )

    var deleteBrowsingHistory by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_delete_browsing_history_now),
        default = true
    )

    var deleteCookies by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_delete_cookies_now),
        default = true
    )

    var deleteCache by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_delete_caches_now),
        default = true
    )

    var deleteSitePermissions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_delete_permissions_now),
        default = true
    )

    var shouldUseBottomToolbar by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_toolbar_bottom),
        // Default accessibility users to top toolbar
        default = !touchExplorationIsEnabled && !switchServiceIsEnabled
    )

    val toolbarPosition: ToolbarPosition
        get() = if (shouldUseBottomToolbar) ToolbarPosition.BOTTOM else ToolbarPosition.TOP

    /**
     * Check each active accessibility service to see if it can perform gestures, if any can,
     * then it is *likely* a switch service is enabled. We are assuming this to be the case based on #7486
     */
    val switchServiceIsEnabled: Boolean
        get() {
            val accessibilityManager =
                appContext.getSystemService(Context.ACCESSIBILITY_SERVICE) as? AccessibilityManager

            accessibilityManager?.getEnabledAccessibilityServiceList(0)?.let { activeServices ->
                for (service in activeServices) {
                    if (service.capabilities.and(CAPABILITY_CAN_PERFORM_GESTURES) == 1) {
                        return true
                    }
                }
            }

            return false
        }

    val touchExplorationIsEnabled: Boolean
        get() {
            val accessibilityManager =
                appContext.getSystemService(Context.ACCESSIBILITY_SERVICE) as? AccessibilityManager
            return accessibilityManager?.isTouchExplorationEnabled ?: false
        }

    val accessibilityServicesEnabled: Boolean
        get() {
            return touchExplorationIsEnabled || switchServiceIsEnabled
        }

    val toolbarSettingString: String
        get() = when {
            shouldUseBottomToolbar -> appContext.getString(R.string.preference_bottom_toolbar)
            else -> appContext.getString(R.string.preference_top_toolbar)
        }

    fun getDeleteDataOnQuit(type: DeleteBrowsingDataOnQuitType): Boolean =
        preferences.getBoolean(type.getPreferenceKey(appContext), false)

    fun setDeleteDataOnQuit(type: DeleteBrowsingDataOnQuitType, value: Boolean) {
        preferences.edit().putBoolean(type.getPreferenceKey(appContext), value).apply()
    }

    fun shouldDeleteAnyDataOnQuit() =
        DeleteBrowsingDataOnQuitType.values().any { getDeleteDataOnQuit(it) }

    val passwordsEncryptionKeyGenerated by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_encryption_key_generated),
        false
    )

    fun recordPasswordsEncryptionKeyGenerated() = preferences.edit().putBoolean(
        appContext.getPreferenceKey(R.string.pref_key_encryption_key_generated),
        true
    ).apply()

    @VisibleForTesting(otherwise = PRIVATE)
    internal val loginsSecureWarningSyncCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_logins_secure_warning_sync),
        default = 0
    )

    @VisibleForTesting(otherwise = PRIVATE)
    internal val loginsSecureWarningCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_logins_secure_warning),
        default = 0
    )

    fun incrementShowLoginsSecureWarningCount() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_logins_secure_warning),
            loginsSecureWarningCount + 1
        ).apply()
    }

    fun incrementShowLoginsSecureWarningSyncCount() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_logins_secure_warning_sync),
            loginsSecureWarningSyncCount + 1
        ).apply()
    }

    val shouldShowSearchSuggestions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions),
        default = true
    )

    var defaultTopSitesAdded by booleanPreference(
        appContext.getPreferenceKey(R.string.default_top_sites_added),
        default = false
    )

    var shouldShowSearchSuggestionsInPrivate by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions_in_private),
        default = false
    )

    var showSearchSuggestionsInPrivateOnboardingFinished by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions_in_private_onboarding),
        default = false
    )

    fun incrementVisitedInstallableCount() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_install_pwa_visits),
            pwaInstallableVisitCount + 1
        ).apply()
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal val pwaInstallableVisitCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_install_pwa_visits),
        default = 0
    )

    private val userNeedsToVisitInstallableSites: Boolean
        get() = pwaInstallableVisitCount < pwaVisitsToShowPromptMaxCount

    val shouldShowPwaOnboarding: Boolean
        get() {
            // We only want to show this on the 3rd time a user visits a site
            if (userNeedsToVisitInstallableSites) return false

            // ShortcutManager::pinnedShortcuts is only available on Oreo+
            if (!userKnowsAboutPwas && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val alreadyHavePwaInstalled =
                    appContext.getSystemService(ShortcutManager::class.java)
                        .pinnedShortcuts.size > 0

                // Users know about PWAs onboarding if they already have PWAs installed.
                userKnowsAboutPwas = alreadyHavePwaInstalled
            }
            // Show dialog only if user does not know abut PWAs
            return !userKnowsAboutPwas
        }

    var userKnowsAboutPwas by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_user_knows_about_pwa),
        default = false
    )

    @VisibleForTesting(otherwise = PRIVATE)
    internal val trackingProtectionOnboardingCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_onboarding),
        0
    )

    fun incrementTrackingProtectionOnboardingCount() {
        trackingProtectionOnboardingShownThisSession = true
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_tracking_protection_onboarding),
            trackingProtectionOnboardingCount + 1
        ).apply()
    }

    fun getSitePermissionsPhoneFeatureAction(
        feature: PhoneFeature,
        default: Action = Action.ASK_TO_ALLOW
    ) =
        preferences.getInt(feature.getPreferenceKey(appContext), default.toInt()).toAction()

    /**
     * Saves the user selected autoplay setting.
     *
     * Under the hood, autoplay is represented by two settings, [AUTOPLAY_AUDIBLE] and
     * [AUTOPLAY_INAUDIBLE]. The user selection cannot be inferred from the combination of these
     * settings because, while on [AUTOPLAY_ALLOW_ON_WIFI], they will be indistinguishable from
     * either [AUTOPLAY_ALLOW_ALL] or [AUTOPLAY_BLOCK_ALL]. Because of this, we are forced to save
     * the user selected setting as well.
     */
    fun setAutoplayUserSetting(
        autoplaySetting: Int
    ) {
        preferences.edit().putInt(AUTOPLAY_USER_SETTING, autoplaySetting).apply()
    }

    /**
     * Gets the user selected autoplay setting.
     *
     * Under the hood, autoplay is represented by two settings, [AUTOPLAY_AUDIBLE] and
     * [AUTOPLAY_INAUDIBLE]. The user selection cannot be inferred from the combination of these
     * settings because, while on [AUTOPLAY_ALLOW_ON_WIFI], they will be indistinguishable from
     * either [AUTOPLAY_ALLOW_ALL] or [AUTOPLAY_BLOCK_ALL]. Because of this, we are forced to save
     * the user selected setting as well.
     */
    fun getAutoplayUserSetting(
        default: Int
    ) = preferences.getInt(AUTOPLAY_USER_SETTING, default)

    fun getSitePermissionsPhoneFeatureAutoplayAction(
        feature: PhoneFeature,
        default: AutoplayAction = AutoplayAction.BLOCKED
    ) = preferences.getInt(feature.getPreferenceKey(appContext), default.toInt()).toAutoplayAction()

    fun setSitePermissionsPhoneFeatureAction(
        feature: PhoneFeature,
        value: Action
    ) {
        preferences.edit().putInt(feature.getPreferenceKey(appContext), value.toInt()).apply()
    }

    fun getSitePermissionsCustomSettingsRules(): SitePermissionsRules {
        return SitePermissionsRules(
            notification = getSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION),
            microphone = getSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE),
            location = getSitePermissionsPhoneFeatureAction(PhoneFeature.LOCATION),
            camera = getSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA),
            autoplayAudible = getSitePermissionsPhoneFeatureAutoplayAction(PhoneFeature.AUTOPLAY_AUDIBLE),
            autoplayInaudible = getSitePermissionsPhoneFeatureAutoplayAction(PhoneFeature.AUTOPLAY_INAUDIBLE)
        )
    }

    fun setSitePermissionSettingListener(lifecycleOwner: LifecycleOwner, listener: () -> Unit) {
        val sitePermissionKeys = listOf(
            PhoneFeature.NOTIFICATION,
            PhoneFeature.MICROPHONE,
            PhoneFeature.LOCATION,
            PhoneFeature.CAMERA,
            PhoneFeature.AUTOPLAY_AUDIBLE,
            PhoneFeature.AUTOPLAY_INAUDIBLE
        ).map { it.getPreferenceKey(appContext) }

        preferences.registerOnSharedPreferenceChangeListener(lifecycleOwner) { _, key ->
            if (key in sitePermissionKeys) listener.invoke()
        }
    }

    var shouldShowVoiceSearch by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_voice_search),
        default = true
    )

    var shouldPromptToSaveLogins by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_save_logins),
        default = true
    )

    var shouldAutofillLogins by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_autofill_logins),
        default = true
    )

    var fxaSignedIn by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_fxa_signed_in),
        default = false
    )

    var fxaHasSyncedItems by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_fxa_has_synced_items),
        default = false
    )

    var lastPlacesStorageMaintenance by longPreference(
        appContext.getPreferenceKey(R.string.pref_key_last_maintenance),
        default = 0
    )

    fun addSearchWidgetInstalled(count: Int) {
        val key = appContext.getPreferenceKey(R.string.pref_key_search_widget_installed)
        val newValue = preferences.getInt(key, 0) + count
        preferences.edit()
            .putInt(key, newValue)
            .apply()
    }

    val searchWidgetInstalled: Boolean
        get() = 0 < preferences.getInt(
            appContext.getPreferenceKey(R.string.pref_key_search_widget_installed),
            0
        )

    fun incrementNumTimesPrivateModeOpened() {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_private_mode_opened),
            numTimesPrivateModeOpened + 1
        ).apply()
    }

    private var showedPrivateModeContextualFeatureRecommender by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_showed_private_mode_cfr),
        default = false
    )

    private val numTimesPrivateModeOpened: Int
        get() = preferences.getInt(
            appContext.getPreferenceKey(R.string.pref_key_private_mode_opened),
            0
        )

    val showPrivateModeContextualFeatureRecommender: Boolean
        get() {
            val focusInstalled = MozillaProductDetector
                .getInstalledMozillaProducts(appContext as Application)
                .contains(MozillaProductDetector.MozillaProducts.FOCUS.productName)

            val showCondition =
                (numTimesPrivateModeOpened == CFR_COUNT_CONDITION_FOCUS_INSTALLED && focusInstalled) ||
                        (numTimesPrivateModeOpened == CFR_COUNT_CONDITION_FOCUS_NOT_INSTALLED && !focusInstalled)

            if (showCondition && !showedPrivateModeContextualFeatureRecommender) {
                showedPrivateModeContextualFeatureRecommender = true
                return true
            }

            return false
        }

    var openLinksInExternalApp by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_open_links_in_external_app),
        default = false
    )

    var overrideFxAServer by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_override_fxa_server),
        default = ""
    )

    var overrideSyncTokenServer by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_override_sync_tokenserver),
        default = ""
    )

    val topSitesSize by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_top_sites_size),
        default = 0
    )

    fun setOpenTabsCount(count: Int) {
        preferences.edit().putInt(
            appContext.getPreferenceKey(R.string.pref_key_open_tabs_count),
            count
        ).apply()
    }

    val openTabsCount: Int
        get() = preferences.getInt(
            appContext.getPreferenceKey(R.string.pref_key_open_tabs_count),
            0
        )

    private var savedLoginsSortingStrategyString by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_saved_logins_sorting_strategy),
        default = SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort.strategyString
    )

    val savedLoginsMenuHighlightedItem: SavedLoginsSortingStrategyMenu.Item
        get() = SavedLoginsSortingStrategyMenu.Item.fromString(savedLoginsSortingStrategyString)

    var savedLoginsSortingStrategy: SortingStrategy
        get() {
            return when (savedLoginsMenuHighlightedItem) {
                SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort ->
                    SortingStrategy.Alphabetically(appContext.components.publicSuffixList)
                SavedLoginsSortingStrategyMenu.Item.LastUsedSort -> SortingStrategy.LastUsed
            }
        }
        set(value) {
            savedLoginsSortingStrategyString = when (value) {
                is SortingStrategy.Alphabetically ->
                    SavedLoginsSortingStrategyMenu.Item.AlphabeticallySort.strategyString
                is SortingStrategy.LastUsed ->
                    SavedLoginsSortingStrategyMenu.Item.LastUsedSort.strategyString
            }
        }
}
