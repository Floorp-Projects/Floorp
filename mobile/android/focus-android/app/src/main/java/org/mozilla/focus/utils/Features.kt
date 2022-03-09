/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

/**
 * Simple feature flags.
 */
object Features {
    const val SEARCH_TERMS_OR_URL: Boolean = true

    /**
     * Delete all shortcuts when New Session Button from FingerPrint LockScreen is clicked
     */
    var DELETE_TOP_SITES_WHEN_NEW_SESSION_BUTTON_CLICKED: Boolean = AppConstants.isDevOrNightlyBuild
}
