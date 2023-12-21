/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.fenix.ext.settings

/**
 * Helper for querying the status and modifying various features and settings in the application.
 */
interface FeatureSettingsHelper {
    /**
     * Whether the onboarding for existing users should be shown or not.
     * It should appear only once on the first visit to homescreen.
     */
    var isHomeOnboardingDialogEnabled: Boolean

    /**
     * Whether the Pocket stories feature is enabled or not.
     */
    var isPocketEnabled: Boolean

    /**
     * Whether the "Jump back in" CFR should be shown or not.
     * It should appear on the first visit to homescreen given that there is a tab opened.
     */
    var isJumpBackInCFREnabled: Boolean

    /**
     * Whether the onboarding dialog for choosing wallpapers should be shown or not.
     */
    var isWallpaperOnboardingEnabled: Boolean

    /**
     * Whether the "Jump back in" homescreen section is enabled or not.
     * It shows the last visited tab on this device and on other synced devices.
     */
    var isRecentTabsFeatureEnabled: Boolean

    /**
     * Whether the "Recently visited" homescreen section is enabled or not.
     * It can show up to 9 history highlights and history groups.
     */
    var isRecentlyVisitedFeatureEnabled: Boolean

    /**
     * Whether the onboarding dialog for PWAs should be shown or not.
     * It can show the first time a website that can be installed as a PWA is accessed.
     */
    var isPWAsPromptEnabled: Boolean

    /**
     * Whether the "Site permissions" option is checked in the "Delete browsing data" screen or not.
     */
    var isDeleteSitePermissionsEnabled: Boolean

    /**
     * Enable or disable showing the TCP CFR when accessing a webpage for the first time.
     */
    var isTCPCFREnabled: Boolean

    /**
     * The current "Enhanced Tracking Protection" policy.
     * @see ETPPolicy
     */
    var etpPolicy: ETPPolicy

    /**
     * Enable or disable open in app banner.
     */
    var isOpenInAppBannerEnabled: Boolean

    /**
     * Enable or disable the Tabs Tray to Compose rewrite.
     */
    var tabsTrayRewriteEnabled: Boolean

    /**
     * Enable or disable the Top Sites to Compose rewrite.
     */
    var composeTopSitesEnabled: Boolean

    /**
     * Enable or disable translations flow.
     */
    var isTranslationsEnabled: Boolean

    fun applyFlagUpdates()

    fun resetAllFeatureFlags()

    companion object {
        val settings = InstrumentationRegistry.getInstrumentation().targetContext.settings()
    }
}

/**
 * All "Enhanced Tracking Protection" modes.
 */
enum class ETPPolicy {
    STANDARD,
    STRICT,
    CUSTOM,
}
