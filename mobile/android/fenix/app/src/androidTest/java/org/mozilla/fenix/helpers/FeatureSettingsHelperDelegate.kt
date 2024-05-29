/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.util.Log
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.ETPPolicy.CUSTOM
import org.mozilla.fenix.helpers.ETPPolicy.STANDARD
import org.mozilla.fenix.helpers.ETPPolicy.STRICT
import org.mozilla.fenix.helpers.FeatureSettingsHelper.Companion.settings
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.utils.Settings

/**
 * Helper for querying the status and modifying various features and settings in the application.
 */
class FeatureSettingsHelperDelegate() : FeatureSettingsHelper {
    /**
     * The current feature flags used inside the app before the tests start.
     * These will be restored when the tests end.
     */
    private val initialFeatureFlags = FeatureFlags(
        isHomeOnboardingDialogEnabled = settings.showHomeOnboardingDialog,
        homeOnboardingDialogVersion = getHomeOnboardingVersion(),
        isPocketEnabled = settings.showPocketRecommendationsFeature,
        isJumpBackInCFREnabled = settings.shouldShowJumpBackInCFR,
        isRecentTabsFeatureEnabled = settings.showRecentTabsFeature,
        isRecentlyVisitedFeatureEnabled = settings.historyMetadataUIFeature,
        isPWAsPromptEnabled = !settings.userKnowsAboutPwas,
        isTCPCFREnabled = settings.shouldShowTotalCookieProtectionCFR,
        isWallpaperOnboardingEnabled = settings.showWallpaperOnboarding,
        isDeleteSitePermissionsEnabled = settings.deleteSitePermissions,
        isOpenInAppBannerEnabled = settings.shouldShowOpenInAppBanner,
        etpPolicy = getETPPolicy(settings),
        composeTopSitesEnabled = settings.enableComposeTopSites,
        isLocationPermissionEnabled = getFeaturePermission(PhoneFeature.LOCATION, settings),
    )

    /**
     * The current feature flags updated in tests.
     */
    private var updatedFeatureFlags = initialFeatureFlags.copy()

    override var isHomeOnboardingDialogEnabled: Boolean
        get() = updatedFeatureFlags.isHomeOnboardingDialogEnabled &&
            FenixOnboarding(appContext).userHasBeenOnboarded()
        set(value) {
            updatedFeatureFlags.isHomeOnboardingDialogEnabled = value
            updatedFeatureFlags.homeOnboardingDialogVersion = when (value) {
                true -> FenixOnboarding.CURRENT_ONBOARDING_VERSION
                false -> 0
            }
        }

    override var isPocketEnabled: Boolean by updatedFeatureFlags::isPocketEnabled
    override var isJumpBackInCFREnabled: Boolean by updatedFeatureFlags::isJumpBackInCFREnabled
    override var isWallpaperOnboardingEnabled: Boolean by updatedFeatureFlags::isWallpaperOnboardingEnabled
    override var isRecentTabsFeatureEnabled: Boolean by updatedFeatureFlags::isRecentTabsFeatureEnabled
    override var isRecentlyVisitedFeatureEnabled: Boolean by updatedFeatureFlags::isRecentlyVisitedFeatureEnabled
    override var isPWAsPromptEnabled: Boolean by updatedFeatureFlags::isPWAsPromptEnabled
    override var isTCPCFREnabled: Boolean by updatedFeatureFlags::isTCPCFREnabled
    override var isOpenInAppBannerEnabled: Boolean by updatedFeatureFlags::isOpenInAppBannerEnabled
    override var etpPolicy: ETPPolicy by updatedFeatureFlags::etpPolicy
    override var composeTopSitesEnabled: Boolean by updatedFeatureFlags::composeTopSitesEnabled
    override var isLocationPermissionEnabled: SitePermissionsRules.Action by updatedFeatureFlags::isLocationPermissionEnabled

    override fun applyFlagUpdates() {
        Log.i(TAG, "applyFlagUpdates: Trying to apply the updated feature flags: $updatedFeatureFlags")
        applyFeatureFlags(updatedFeatureFlags)
        Log.i(TAG, "applyFlagUpdates: Applied the updated feature flags: $updatedFeatureFlags")
    }

    override fun resetAllFeatureFlags() {
        Log.i(TAG, "resetAllFeatureFlags: Trying to reset the feature flags to: $initialFeatureFlags")
        applyFeatureFlags(initialFeatureFlags)
        Log.i(TAG, "resetAllFeatureFlags: Performed feature flags reset to: $initialFeatureFlags")
    }

    override var isDeleteSitePermissionsEnabled: Boolean by updatedFeatureFlags::isDeleteSitePermissionsEnabled

    private fun applyFeatureFlags(featureFlags: FeatureFlags) {
        settings.showHomeOnboardingDialog = featureFlags.isHomeOnboardingDialogEnabled
        setHomeOnboardingVersion(featureFlags.homeOnboardingDialogVersion)
        settings.showPocketRecommendationsFeature = featureFlags.isPocketEnabled
        settings.shouldShowJumpBackInCFR = featureFlags.isJumpBackInCFREnabled
        settings.showRecentTabsFeature = featureFlags.isRecentTabsFeatureEnabled
        settings.historyMetadataUIFeature = featureFlags.isRecentlyVisitedFeatureEnabled
        settings.userKnowsAboutPwas = !featureFlags.isPWAsPromptEnabled
        settings.shouldShowTotalCookieProtectionCFR = featureFlags.isTCPCFREnabled
        settings.showWallpaperOnboarding = featureFlags.isWallpaperOnboardingEnabled
        settings.deleteSitePermissions = featureFlags.isDeleteSitePermissionsEnabled
        settings.shouldShowOpenInAppBanner = featureFlags.isOpenInAppBannerEnabled
        settings.enableComposeTopSites = featureFlags.composeTopSitesEnabled
        setETPPolicy(featureFlags.etpPolicy)
        setPermissions(PhoneFeature.LOCATION, featureFlags.isLocationPermissionEnabled)
    }
}

private data class FeatureFlags(
    var isHomeOnboardingDialogEnabled: Boolean,
    var homeOnboardingDialogVersion: Int,
    var isPocketEnabled: Boolean,
    var isJumpBackInCFREnabled: Boolean,
    var isRecentTabsFeatureEnabled: Boolean,
    var isRecentlyVisitedFeatureEnabled: Boolean,
    var isPWAsPromptEnabled: Boolean,
    var isTCPCFREnabled: Boolean,
    var isWallpaperOnboardingEnabled: Boolean,
    var isDeleteSitePermissionsEnabled: Boolean,
    var isOpenInAppBannerEnabled: Boolean,
    var etpPolicy: ETPPolicy,
    var composeTopSitesEnabled: Boolean,
    var isLocationPermissionEnabled: SitePermissionsRules.Action,
)

internal fun getETPPolicy(settings: Settings): ETPPolicy {
    return when {
        settings.useStrictTrackingProtection -> STRICT
        settings.useCustomTrackingProtection -> CUSTOM
        else -> STANDARD
    }
}

private fun setETPPolicy(policy: ETPPolicy) {
    when (policy) {
        STRICT -> {
            Log.i(TAG, "setETPPolicy: Trying to set ETP policy to: \"Strict\"")
            settings.setStrictETP()
            Log.i(TAG, "setETPPolicy: ETP policy was set to: \"Strict\"")
        }
        // The following two cases update ETP in the same way "setStrictETP" does.
        STANDARD -> {
            Log.i(TAG, "setETPPolicy: Trying to set ETP policy to: \"Standard\"")
            settings.preferences.edit()
                .putBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_tracking_protection_strict_default),
                    false,
                )
                .putBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_option),
                    false,
                )
                .putBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_tracking_protection_standard_option),
                    true,
                )
                .commit()
            Log.i(TAG, "setETPPolicy: ETP policy was set to: \"Standard\"")
        }
        CUSTOM -> {
            Log.i(TAG, "setETPPolicy: Trying to set ETP policy to: \"Custom\"")
            settings.preferences.edit()
                .putBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_tracking_protection_strict_default),
                    false,
                )
                .putBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_tracking_protection_standard_option),
                    true,
                )
                .putBoolean(
                    appContext.getPreferenceKey(R.string.pref_key_tracking_protection_custom_option),
                    true,
                )
                .commit()
            Log.i(TAG, "setETPPolicy: ETP policy was set to: \"Custom\"")
        }
    }
}

private fun getHomeOnboardingVersion(): Int {
    Log.i(TAG, "getHomeOnboardingVersion: Trying to get the onboarding version")
    return FenixOnboarding(appContext)
        .preferences
        .getInt(FenixOnboarding.LAST_VERSION_ONBOARDING_KEY, 0)
}

private fun setHomeOnboardingVersion(version: Int) {
    Log.i(TAG, "setHomeOnboardingVersion: Trying to set the onboarding version to: $version")
    FenixOnboarding(appContext)
        .preferences.edit()
        .putInt(FenixOnboarding.LAST_VERSION_ONBOARDING_KEY, version)
        .commit()
    Log.i(TAG, "setHomeOnboardingVersion: Onboarding version was set to: $version")
}

internal fun getFeaturePermission(feature: PhoneFeature, settings: Settings): SitePermissionsRules.Action {
    Log.i(TAG, "getFeaturePermission: The default permission for $feature is ${feature.getAction(settings)}.")
    return feature.getAction(settings)
}

private fun setPermissions(feature: PhoneFeature, action: SitePermissionsRules.Action) {
    runBlocking {
        Log.i(TAG, "setPermissions: Trying to set $action permission for $feature.")
        appContext.settings().setSitePermissionsPhoneFeatureAction(feature, action)
        Log.i(TAG, "setPermissions: Set $action permission for $feature.")
    }
}
