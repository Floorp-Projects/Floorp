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
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.MozillaProductDetector
import org.mozilla.fenix.components.settings.counterPreference
import org.mozilla.fenix.components.settings.featureFlagPreference
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.experiments.ExperimentBranch
import org.mozilla.fenix.experiments.FeatureId
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.withExperiment
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType
import org.mozilla.fenix.settings.logins.SavedLoginsSortingStrategyMenu
import org.mozilla.fenix.settings.logins.SortingStrategy
import org.mozilla.fenix.settings.registerOnSharedPreferenceChangeListener
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_BLOCK_ALL
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_BLOCK_AUDIBLE
import java.security.InvalidParameterException

private const val AUTOPLAY_USER_SETTING = "AUTOPLAY_USER_SETTING"

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 * @param appContext Reference to application context.
 */
@Suppress("LargeClass", "TooManyFunctions")
class Settings(private val appContext: Context) : PreferencesHolder {

    companion object {
        const val topSitesMaxCount = 16
        const val FENIX_PREFERENCES = "fenix_preferences"

        private const val BLOCKED_INT = 0
        private const val ASK_TO_ALLOW_INT = 1
        private const val ALLOWED_INT = 2
        private const val CFR_COUNT_CONDITION_FOCUS_INSTALLED = 1
        private const val CFR_COUNT_CONDITION_FOCUS_NOT_INSTALLED = 3
        private const val APP_LAUNCHES_TO_SHOW_DEFAULT_BROWSER_CARD = 3

        const val FOUR_HOURS_MS = 60 * 60 * 4 * 1000L
        const val ONE_DAY_MS = 60 * 60 * 24 * 1000L
        const val THREE_DAYS_MS = 3 * ONE_DAY_MS
        const val ONE_WEEK_MS = 60 * 60 * 24 * 7 * 1000L
        const val ONE_MONTH_MS = (60 * 60 * 24 * 365 * 1000L) / 12

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

    var showTopFrecentSites by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_enable_top_frecent_sites),
        default = true
    )

    var numberOfAppLaunches by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_times_app_opened),
        default = 0
    )

    var lastReviewPromptTimeInMillis by longPreference(
        appContext.getPreferenceKey(R.string.pref_key_last_review_prompt_shown_time),
        default = 0L
    )

    var lastCfrShownTimeInMillis by longPreference(
        appContext.getPreferenceKey(R.string.pref_key_last_cfr_shown_time),
        default = 0L
    )

    val canShowCfr: Boolean
        get() = (System.currentTimeMillis() - lastCfrShownTimeInMillis) > THREE_DAYS_MS

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

    var shouldDisplayMasterPasswordMigrationTip by booleanPreference(
        appContext.getString(R.string.pref_key_master_password_tip),
        true
    )

    var shouldReturnToBrowser by booleanPreference(
        appContext.getString(R.string.pref_key_return_to_browser),
        false
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

    var showCollectionsPlaceholderOnHome by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_collections_placeholder_home),
        default = true
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

    var isExperimentationEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_experimentation),
        default = true
    )

    var isOverrideTPPopupsForPerformanceTest = false

    var showSecretDebugMenuThisSession = false

    val shouldShowSecurityPinWarningSync: Boolean
        get() = loginsSecureWarningSyncCount.underMaxCount()

    val shouldShowSecurityPinWarning: Boolean
        get() = secureWarningCount.underMaxCount()

    var shouldShowPrivacyPopWindow by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_privacy_pop_window),
        default = true
    )

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

    val shouldShowSyncedTabsSuggestions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_search_synced_tabs),
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

    /**
     * Shows if the user has chosen to close the set default browser experiment card
     * on home screen or has clicked the set as default browser button.
     */
    var userDismissedExperimentCard by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_experiment_card_home),
        default = false
    )

    /**
     * Shows if the set default browser experiment card should be shown on home screen.
     */
    fun shouldShowSetAsDefaultBrowserCard(): Boolean {
        val browsers = BrowsersCache.all(appContext)
        val experiments = appContext.components.analytics.experiments
        val isExperimentBranch =
            experiments.withExperiment(FeatureId.DEFAULT_BROWSER) { experimentBranch ->
                (experimentBranch == ExperimentBranch.DEFAULT_BROWSER_NEW_TAB_BANNER)
            }
        return isExperimentBranch == true &&
            !userDismissedExperimentCard &&
            !browsers.isFirefoxDefaultBrowser &&
            numberOfAppLaunches > APP_LAUNCHES_TO_SHOW_DEFAULT_BROWSER_CARD
    }

    var gridTabView by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tab_view_grid),
        default = true
    )

    var manuallyCloseTabs by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_close_tabs_manually),
        default = true
    )

    var closeTabsAfterOneDay by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_close_tabs_after_one_day),
        default = false
    )

    var closeTabsAfterOneWeek by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_close_tabs_after_one_week),
        default = false
    )

    var closeTabsAfterOneMonth by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_close_tabs_after_one_month),
        default = false
    )

    var allowThirdPartyRootCerts by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_allow_third_party_root_certs),
        default = false
    )

    var nimbusUsePreview by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_nimbus_use_preview),
        default = false
    )

    /**
     * Indicates the last time when the user was interacting with the [BrowserFragment],
     * This is useful to determine if the user has to start on the [HomeFragment]
     * or it should go directly to the [BrowserFragment].
     */
    var lastBrowseActivity by longPreference(
        appContext.getPreferenceKey(R.string.pref_key_last_browse_activity_time),
        default = timeNowInMillis()
    )

    /**
     * Indicates if the user has selected the option to start on the home screen after
     * four hours of inactivity.
     */
    var startOnHomeAfterFourHours by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_start_on_home_after_four_hours),
        default = true
    )

    /**
     * Indicates if the user has selected the option to always start on the home screen.
     */
    var startOnHomeAlways by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_start_on_home_always),
        default = false
    )

    /**
     * Indicates if the user has selected the option to never start on the home screen.
     */
    var startOnHomeNever by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_start_on_home_never),
        default = false
    )

    /**
     * Indicates if the user should start on the home screen, based on the user's preferences.
     */
    fun shouldStartOnHome(): Boolean {
        return when {
            startOnHomeAfterFourHours -> timeNowInMillis() - lastBrowseActivity >= FOUR_HOURS_MS
            startOnHomeAlways -> true
            startOnHomeNever -> false
            else -> false
        }
    }

    @VisibleForTesting
    internal fun timeNowInMillis(): Long = System.currentTimeMillis()

    fun getTabTimeout(): Long = when {
        closeTabsAfterOneDay -> ONE_DAY_MS
        closeTabsAfterOneWeek -> ONE_WEEK_MS
        closeTabsAfterOneMonth -> ONE_MONTH_MS
        else -> Long.MAX_VALUE
    }

    enum class TabView {
        GRID, LIST
    }

    fun getTabViewPingString() = if (gridTabView) TabView.GRID.name else TabView.LIST.name

    enum class TabTimout {
        ONE_DAY, ONE_WEEK, ONE_MONTH, MANUAL
    }

    fun getTabTimeoutPingString(): String = when {
        closeTabsAfterOneDay -> {
            TabTimout.ONE_DAY.name
        }
        closeTabsAfterOneWeek -> {
            TabTimout.ONE_WEEK.name
        }
        closeTabsAfterOneMonth -> {
            TabTimout.ONE_MONTH.name
        }
        else -> {
            TabTimout.MANUAL.name
        }
    }

    fun getTabTimeoutString(): String = when {
        closeTabsAfterOneDay -> {
            appContext.getString(R.string.close_tabs_after_one_day_summary)
        }
        closeTabsAfterOneWeek -> {
            appContext.getString(R.string.close_tabs_after_one_week_summary)
        }
        closeTabsAfterOneMonth -> {
            appContext.getString(R.string.close_tabs_after_one_month_summary)
        }
        else -> {
            appContext.getString(R.string.close_tabs_manually_summary)
        }
    }

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
     * Declared as a function for performance purposes. This could be declared as a variable using
     * booleanPreference like other members of this class. However, doing so will make it so it will
     * be initialized once Settings.kt is first called, which in turn will call `isDefaultBrowserBlocking()`.
     * This will lead to a performance regression since that function can be expensive to call.
     */
    fun checkIfFenixIsDefaultBrowserOnAppResume(): Boolean {
        val prefKey = appContext.getPreferenceKey(R.string.pref_key_default_browser)
        val isDefaultBrowserNow = isDefaultBrowserBlocking()
        val wasDefaultBrowserOnLastResume = this.preferences.getBoolean(prefKey, isDefaultBrowserNow)
        this.preferences.edit().putBoolean(prefKey, isDefaultBrowserNow).apply()
        return isDefaultBrowserNow && !wasDefaultBrowserOnLastResume
    }

    /**
     * This function is "blocking" since calling this can take approx. 30-40ms (timing taken on a
     * G5+).
     */
    fun isDefaultBrowserBlocking(): Boolean {
        val browsers = BrowsersCache.all(appContext)
        return browsers.isDefaultBrowser
    }

    var defaultBrowserNotificationDisplayed by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_should_show_default_browser_notification),
        default = false
    )

    fun shouldShowDefaultBrowserNotification(): Boolean {
        return !defaultBrowserNotificationDisplayed && !isDefaultBrowserBlocking()
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

    val blockRedirectTrackersInCustomTrackingProtection by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_tracking_protection_redirect_trackers),
        true
    )

    /**
     * Prefer to use a fixed top toolbar when:
     * - a talkback service is enabled or
     * - switch access is enabled.
     *
     * This is automatically inferred based on the current system status. Not a setting in our app.
     */
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

    var deleteDownloads by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_delete_downloads_now),
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
    internal val loginsSecureWarningSyncCount = counterPreference(
        appContext.getPreferenceKey(R.string.pref_key_logins_secure_warning_sync),
        maxCount = 1
    )

    @VisibleForTesting(otherwise = PRIVATE)
    internal val secureWarningCount = counterPreference(
        appContext.getPreferenceKey(R.string.pref_key_secure_warning),
        maxCount = 1
    )

    fun incrementSecureWarningCount() = secureWarningCount.increment()

    fun incrementShowLoginsSecureWarningSyncCount() = loginsSecureWarningSyncCount.increment()

    val shouldShowSearchSuggestions by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_search_suggestions),
        default = true
    )

    val shouldAutocompleteInAwesomebar by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_enable_autocomplete_urls),
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

    fun incrementVisitedInstallableCount() = pwaInstallableVisitCount.increment()

    @VisibleForTesting(otherwise = PRIVATE)
    internal val pwaInstallableVisitCount = counterPreference(
        appContext.getPreferenceKey(R.string.pref_key_install_pwa_visits),
        maxCount = 3
    )

    private val userNeedsToVisitInstallableSites: Boolean
        get() = pwaInstallableVisitCount.underMaxCount()

    val shouldShowPwaCfr: Boolean
        get() {
            if (!canShowCfr) return false
            // We only want to show this on the 3rd time a user visits a site
            if (userNeedsToVisitInstallableSites) return false

            // ShortcutManager::pinnedShortcuts is only available on Oreo+
            if (!userKnowsAboutPwas && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val manager = appContext.getSystemService(ShortcutManager::class.java)
                val alreadyHavePwaInstalled = manager != null && manager.pinnedShortcuts.size > 0

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

    var shouldShowOpenInAppBanner by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_should_show_open_in_app_banner),
        default = true
    )

    val shouldShowOpenInAppCfr: Boolean
        get() = canShowCfr && shouldShowOpenInAppBanner

    var shouldShowAutoCloseTabsBanner by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_should_show_auto_close_tabs_banner),
        default = true
    )

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
    fun getAutoplayUserSetting() = preferences.getInt(AUTOPLAY_USER_SETTING, AUTOPLAY_BLOCK_AUDIBLE)

    private fun getSitePermissionsPhoneFeatureAutoplayAction(
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
            autoplayAudible = getSitePermissionsPhoneFeatureAutoplayAction(
                feature = PhoneFeature.AUTOPLAY_AUDIBLE,
                default = AutoplayAction.BLOCKED
            ),
            autoplayInaudible = getSitePermissionsPhoneFeatureAutoplayAction(
                feature = PhoneFeature.AUTOPLAY_INAUDIBLE,
                default = AutoplayAction.ALLOWED
            ),
            persistentStorage = getSitePermissionsPhoneFeatureAction(PhoneFeature.PERSISTENT_STORAGE),
            mediaKeySystemAccess = getSitePermissionsPhoneFeatureAction(PhoneFeature.MEDIA_KEY_SYSTEM_ACCESS)
        )
    }

    fun setSitePermissionSettingListener(lifecycleOwner: LifecycleOwner, listener: () -> Unit) {
        val sitePermissionKeys = listOf(
            PhoneFeature.NOTIFICATION,
            PhoneFeature.MICROPHONE,
            PhoneFeature.LOCATION,
            PhoneFeature.CAMERA,
            PhoneFeature.AUTOPLAY_AUDIBLE,
            PhoneFeature.AUTOPLAY_INAUDIBLE,
            PhoneFeature.PERSISTENT_STORAGE,
            PhoneFeature.MEDIA_KEY_SYSTEM_ACCESS
        ).map { it.getPreferenceKey(appContext) }

        preferences.registerOnSharedPreferenceChangeListener(lifecycleOwner) { _, key ->
            if (key in sitePermissionKeys) listener.invoke()
        }
    }

    var shouldShowVoiceSearch by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_voice_search),
        default = true
    )

    /**
     * Used in [SearchDialogFragment.kt], [SearchFragment.kt] (deprecated), and [PairFragment.kt]
     * to see if we need to check for camera permissions before using the QR code scanner.
     */
    var shouldShowCameraPermissionPrompt by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_camera_permissions_needed),
        default = true
    )

    /**
     * Sets the state of permissions that have been checked, where [false] denotes already checked
     * and [true] denotes needing to check. See [shouldShowCameraPermissionPrompt].
     */
    var setCameraPermissionNeededState by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_camera_permissions_needed),
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

    fun incrementNumTimesPrivateModeOpened() = numTimesPrivateModeOpened.increment()

    var showedPrivateModeContextualFeatureRecommender by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_showed_private_mode_cfr),
        default = false
    )

    private val numTimesPrivateModeOpened = counterPreference(
        appContext.getPreferenceKey(R.string.pref_key_private_mode_opened)
    )

    val shouldShowPrivateModeCfr: Boolean
        get() {
            if (!canShowCfr) return false
            val focusInstalled = MozillaProductDetector
                .getInstalledMozillaProducts(appContext as Application)
                .contains(MozillaProductDetector.MozillaProducts.FOCUS.productName)

            val showCondition = if (focusInstalled) {
                numTimesPrivateModeOpened.value >= CFR_COUNT_CONDITION_FOCUS_INSTALLED
            } else {
                numTimesPrivateModeOpened.value >= CFR_COUNT_CONDITION_FOCUS_NOT_INSTALLED
            }

            if (showCondition && !showedPrivateModeContextualFeatureRecommender) {
                return true
            }

            return false
        }

    var openLinksInExternalApp by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_open_links_in_external_app),
        default = false
    )

    var allowDomesticChinaFxaServer by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_allow_domestic_china_fxa_server),
        default = true
    )

    var overrideFxAServer by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_override_fxa_server),
        default = ""
    )

    var overrideSyncTokenServer by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_override_sync_tokenserver),
        default = ""
    )

    var overrideAmoUser by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_override_amo_user),
        default = ""
    )

    var overrideAmoCollection by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_override_amo_collection),
        default = ""
    )

    fun amoCollectionOverrideConfigured(): Boolean {
        return overrideAmoUser.isNotEmpty() || overrideAmoCollection.isNotEmpty()
    }

    var topSitesSize by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_top_sites_size),
        default = 0
    )

    val topSitesMaxLimit by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_top_sites_max_limit),
        default = topSitesMaxCount
    )

    var openTabsCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_open_tabs_count),
        0
    )

    var mobileBookmarksSize by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_mobile_bookmarks_size),
        0
    )

    var desktopBookmarksSize by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_desktop_bookmarks_size),
        0
    )

    /**
     * Storing number of installed add-ons for telemetry purposes
     */
    var installedAddonsCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_installed_addons_count),
        0
    )

    /**
     * Storing the list of installed add-ons for telemetry purposes
     */
    var installedAddonsList by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_installed_addons_list),
        default = ""
    )

    /**
     * Storing number of enabled add-ons for telemetry purposes
     */
    var enabledAddonsCount by intPreference(
        appContext.getPreferenceKey(R.string.pref_key_enabled_addons_count),
        0
    )

    /**
     * Storing the list of enabled add-ons for telemetry purposes
     */
    var enabledAddonsList by stringPreference(
        appContext.getPreferenceKey(R.string.pref_key_enabled_addons_list),
        default = ""
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

    var isPullToRefreshEnabledInBrowser by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_website_pull_to_refresh),
        default = true
    )

    var isDynamicToolbarEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_dynamic_toolbar),
        default = true
    )

    var isSwipeToolbarToSwitchTabsEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_swipe_toolbar_switch_tabs),
        default = true
    )

    var addressFeature by featureFlagPreference(
        appContext.getPreferenceKey(R.string.pref_key_show_address_feature),
        default = false,
        featureFlag = FeatureFlags.addressesFeature
    )

    var isHistoryMetadataEnabled by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_history_metadata_feature),
        default = false
    )

    var historyMetadataUIFeature by featureFlagPreference(
        appContext.getPreferenceKey(R.string.pref_key_history_metadata_feature),
        default = FeatureFlags.historyMetadataUIFeature,
        featureFlag = FeatureFlags.historyMetadataUIFeature || isHistoryMetadataEnabled
    )

    /**
     * Storing desktop item checkbox value in the home screen menu.
     * If set to true, next opened tab from home screen will be opened in desktop mode.
     */
    var openNextTabInDesktopMode by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_open_next_tab_desktop_mode),
        default = false
    )

    var signedInFxaAccount by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_fxa_signed_in),
        default = false
    )

    /**
     * Storing the user choice from the "Credit cards" settings for whether save and autofill cards
     * should be enabled or not.
     * If set to `true` when the user focuses on credit card fields in the webpage an Android prompt letting her
     * select the card details to be automatically filled will appear.
     */
    var shouldAutofillCreditCardDetails by booleanPreference(
        appContext.getPreferenceKey(R.string.pref_key_credit_cards_save_and_autofill_cards),
        default = true
    )
}
