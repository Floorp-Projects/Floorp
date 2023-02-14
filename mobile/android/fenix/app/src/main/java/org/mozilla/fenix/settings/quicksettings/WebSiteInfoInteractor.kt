/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

/**
 * Contract declaring all possible user interactions with [WebsitePermissionsView].
 */
interface WebSiteInfoInteractor {
    /**
     * Indicates there are website permissions allowed / blocked for the current website.
     * which, status which is shown to the user.
     */
    fun onConnectionDetailsClicked() = Unit

    /**
     * Called whenever back is pressed.
     */
    fun onBackPressed() = Unit
}
