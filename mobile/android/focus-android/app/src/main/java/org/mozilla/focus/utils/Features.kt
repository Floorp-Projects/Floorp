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
    /**
     * HTTPS-Only mode.
     * https://support.mozilla.org/en-US/kb/https-only-prefs-focus
     */
    val HTTPS_ONLY_MODE = AppConstants.isDevOrNightlyBuild
}
