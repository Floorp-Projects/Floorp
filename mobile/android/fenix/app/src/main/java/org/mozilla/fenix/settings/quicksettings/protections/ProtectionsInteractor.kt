/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections

/**
 * Contract declaring all possible user interactions with [ProtectionsView].
 */
interface ProtectionsInteractor {

    /**
     * Called whenever the tracking protection toggle for this site is toggled.
     *
     * @param isEnabled Whether or not tracking protection is enabled.
     */
    fun onTrackingProtectionToggled(isEnabled: Boolean)

    /**
     * Navigates to the tracking protection details panel.
     */
    fun onCookieBannerHandlingDetailsClicked()

    /**
     * Navigates to the tracking protection preferences. Called when a user clicks on the
     * "Details" button.
     */
    fun onTrackingProtectionDetailsClicked()
}
