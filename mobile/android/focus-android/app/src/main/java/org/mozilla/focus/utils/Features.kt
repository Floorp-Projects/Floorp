/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

/**
 * Simple feature flags.
 */
object Features {
    const val SEARCH_TERMS_OR_URL: Boolean = true
    val SHOULD_SHOW_CFR_FOR_SHIELD_TOOLBAR_ICON = AppConstants.isDevOrNightlyBuild
    val SHOULD_SHOW_TOOLTIP_FOR_PRIVACY_SECURITY_SETTINGS_SCREEN = AppConstants.isDevOrNightlyBuild
    const val SHOULD_SHOW_HOME_PAGE_PRO_TIPS = true

    /**
     * Erase tabs CFR
     */
    var SHOW_ERASE_CFR = AppConstants.isDevOrNightlyBuild

    /**
     * Replace the current [FirstrunFragment.kt] which has multiple cards with
     * [OnboardingFragment.kt] which has a new design
     * When this flag it will be removed all classes, methods, tests, etc. which
     * has "firstrun" in their name should be renamed to use "onboarding"
     */
    val ONBOARDING = AppConstants.isDevOrNightlyBuild
}
