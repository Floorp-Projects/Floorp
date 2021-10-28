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

    fun setPocketEnabled(enabled: Boolean) {
        settings.showPocketRecommendationsFeature = enabled
    }

    fun setJumpBackCFREnabled(enabled: Boolean) {
        settings.shouldShowJumpBackInCFR = enabled
    }

    fun setRecentTabsFeatureEnabled(enabled: Boolean) {
        settings.showRecentTabsFeature = enabled
    }

    // Important:
    // Use this after each test if you have modified these feature settings
    // to make sure the app goes back to the default state
    fun resetAllFeatureFlags() {
        settings.showPocketRecommendationsFeature = isPocketEnabled
        settings.shouldShowJumpBackInCFR = isJumpBackInCFREnabled
        settings.showRecentTabsFeature = isRecentTabsFeatureEnabled
    }
}
