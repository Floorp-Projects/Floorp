/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import org.mozilla.fenix.settings.quicksettings.protections.ProtectionsInteractor

/**
 * [QuickSettingsSheetDialogFragment] interactor.
 *
 * Implements callbacks for each of [QuickSettingsSheetDialogFragment]'s Views declared possible user interactions,
 * delegates all such user events to the [QuickSettingsController].
 *
 * @param controller [QuickSettingsController] which will be delegated for all users interactions,
 * it expected to contain all business logic for how to act in response.
 */
class QuickSettingsInteractor(
    private val controller: QuickSettingsController,
) : WebsitePermissionInteractor, ProtectionsInteractor, WebSiteInfoInteractor, ClearSiteDataViewInteractor {
    override fun onPermissionsShown() {
        controller.handlePermissionsShown()
    }

    override fun onPermissionToggled(permissionState: WebsitePermission) {
        controller.handlePermissionToggled(permissionState)
    }

    override fun onAutoplayChanged(value: AutoplayValue) {
        controller.handleAutoplayChanged(value)
    }

    override fun onTrackingProtectionToggled(isEnabled: Boolean) {
        controller.handleTrackingProtectionToggled(isEnabled)
    }

    override fun onCookieBannerHandlingDetailsClicked() {
        controller.handleCookieBannerHandlingDetailsClicked()
    }

    override fun onTrackingProtectionDetailsClicked() {
        controller.handleTrackingProtectionDetailsClicked()
    }

    override fun onConnectionDetailsClicked() {
        controller.handleConnectionDetailsClicked()
    }

    override fun onClearSiteDataClicked(baseDomain: String) {
        controller.handleClearSiteDataClicked(baseDomain)
    }
}
