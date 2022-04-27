/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

/**
 * Simple feature flags.
 */
object Features {
    const val SEARCH_TERMS_OR_URL: Boolean = true
    val IS_TRACKING_PROTECTION_CFR_ENABLED = AppConstants.isDevOrNightlyBuild
    val IS_TOOLTIP_FOR_PRIVACY_SECURITY_SETTINGS_SCREEN_ENABLED = AppConstants.isDevOrNightlyBuild
    const val ARE_HOME_PAGE_PRO_TIPS_ENABLED = true

    /**
     * Erase tabs CFR
     */
    val IS_ERASE_CFR_ENABLED = AppConstants.isDevOrNightlyBuild

    /**
     * Replace the current [FirstrunFragment.kt] which has multiple cards with
     * [OnboardingFragment.kt] which has a new design
     * When this flag it will be removed all classes, methods, tests, etc. which
     * has "firstrun" in their name should be renamed to use "onboarding"
     */
    var ONBOARDING = AppConstants.isDevOrNightlyBuild

    /**
     * Delete all shortcuts when New Session Button from FingerPrint LockScreen is clicked
     */
    var DELETE_TOP_SITES_WHEN_NEW_SESSION_BUTTON_CLICKED: Boolean = AppConstants.isDevOrNightlyBuild
}
