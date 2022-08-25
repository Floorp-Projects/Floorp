/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.content.Context
import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.fenix.ext.settings

class FeatureSettingsHelper {
    private val context: Context = InstrumentationRegistry.getInstrumentation().targetContext
    private val settings = context.settings()

    // saving default values of feature flags
    private var isPocketEnabled: Boolean = settings.showPocketRecommendationsFeature
    private var isJumpBackInCFREnabled: Boolean = settings.shouldShowJumpBackInCFR
    private var isRecentTabsFeatureEnabled: Boolean = settings.showRecentTabsFeature
    private var isRecentlyVisitedFeatureEnabled: Boolean = settings.historyMetadataUIFeature
    private var isUserKnowsAboutPwasTrue: Boolean = settings.userKnowsAboutPwas
    private var isTCPCFREnabled: Boolean = settings.shouldShowTotalCookieProtectionCFR

    fun setPocketEnabled(enabled: Boolean) {
        settings.showPocketRecommendationsFeature = enabled
    }

    fun setJumpBackCFREnabled(enabled: Boolean) {
        settings.shouldShowJumpBackInCFR = enabled
    }

    fun setRecentTabsFeatureEnabled(enabled: Boolean) {
        settings.showRecentTabsFeature = enabled
    }

    fun setRecentlyVisitedFeatureEnabled(enabled: Boolean) {
        settings.historyMetadataUIFeature = enabled
    }

    fun setStrictETPEnabled() {
        settings.setStrictETP()
    }

    fun disablePwaCFR(disable: Boolean) {
        settings.userKnowsAboutPwas = disable
    }

    fun deleteSitePermissions(delete: Boolean) {
        settings.deleteSitePermissions = delete
    }

    /**
     * Enable or disable showing the TCP CFR when accessing a webpage for the first time.
     */
    fun setTCPCFREnabled(shouldShowCFR: Boolean) {
        settings.shouldShowTotalCookieProtectionCFR = shouldShowCFR
    }

    // Important:
    // Use this after each test if you have modified these feature settings
    // to make sure the app goes back to the default state
    fun resetAllFeatureFlags() {
        settings.showPocketRecommendationsFeature = isPocketEnabled
        settings.shouldShowJumpBackInCFR = isJumpBackInCFREnabled
        settings.showRecentTabsFeature = isRecentTabsFeatureEnabled
        settings.historyMetadataUIFeature = isRecentlyVisitedFeatureEnabled
        settings.userKnowsAboutPwas = isUserKnowsAboutPwasTrue
        settings.shouldShowTotalCookieProtectionCFR = isTCPCFREnabled
    }
}
