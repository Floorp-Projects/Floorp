/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers

import android.content.Context
import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.focus.ext.settings
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.Features

class FeatureSettingsHelper {
    val context: Context = InstrumentationRegistry.getInstrumentation().targetContext
    private val settings = context.settings

    // saving default values of feature flags
    private var shouldShowCfrForShieldToolbarIcon: Boolean = settings.shouldShowCfrForShieldToolbarIcon

    // saving default value of number of tabs opened, which is used for erase tabs cfr
    private var numberOfTabsOpened: Int = settings.numberOfTabsOpened

    fun setShieldIconCFREnabled(enabled: Boolean) {
        settings.shouldShowCfrForShieldToolbarIcon = enabled
    }

    fun setNumberOfTabsOpened(tabsOpened: Int) {
        settings.numberOfTabsOpened = tabsOpened
    }

    fun showNewOnboardingScreen(shouldShow: Boolean) {
        Features.ONBOARDING = shouldShow
    }

    // Important:
    // Use this after each test if you have modified these feature settings
    // to make sure the app goes back to the default state
    fun resetAllFeatureFlags() {
        settings.shouldShowCfrForShieldToolbarIcon = shouldShowCfrForShieldToolbarIcon
        settings.numberOfTabsOpened = numberOfTabsOpened
        Features.ONBOARDING = AppConstants.isDevOrNightlyBuild
    }
}
