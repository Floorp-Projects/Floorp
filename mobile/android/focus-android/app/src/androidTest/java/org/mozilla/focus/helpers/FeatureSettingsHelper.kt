/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers

import android.content.Context
import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.focus.cookiebanner.CookieBannerOption
import org.mozilla.focus.ext.settings

class FeatureSettingsHelper {
    val context: Context = InstrumentationRegistry.getInstrumentation().targetContext
    private val settings = context.settings

    // saving default values of feature flags
    private var shouldShowCfrForTrackingProtection: Boolean =
        settings.shouldShowCfrForTrackingProtection

    fun setCfrForTrackingProtectionEnabled(enabled: Boolean) {
        settings.shouldShowCfrForTrackingProtection = enabled
    }

    fun setShowStartBrowsingCfrEnabled(enabled: Boolean) {
        settings.shouldShowStartBrowsingCfr = enabled
    }

    fun setCookieBannerReductionEnabled(enabled: Boolean) {
        settings.isCookieBannerEnable = enabled
        if (enabled) {
            settings.saveCurrentCookieBannerOptionInSharePref(CookieBannerOption.CookieBannerRejectAll())
        } else {
            settings.saveCurrentCookieBannerOptionInSharePref(CookieBannerOption.CookieBannerDisabled())
        }
    }

    fun setSearchWidgetDialogEnabled(enabled: Boolean) {
        if (enabled) {
            settings.addClearBrowsingSessions(4)
        } else {
            settings.addClearBrowsingSessions(10)
        }
    }

    // Important:
    // Use this after each test if you have modified these feature settings
    // to make sure the app goes back to the default state
    fun resetAllFeatureFlags() {
        settings.shouldShowCfrForTrackingProtection = shouldShowCfrForTrackingProtection
    }
}
