/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

class QuickSettingsInteractor(
    private val controller: QuickSettingsController
) : WebsitePermissionInteractor, TrackingProtectionInteractor {

    override fun onReportProblemSelected(websiteUrl: String) {
        controller.handleReportTrackingProblem(websiteUrl)
    }

    override fun onProtectionToggled(websiteUrl: String, trackingEnabled: Boolean) {
        controller.handleTrackingProtectionToggled(websiteUrl, trackingEnabled)
    }

    override fun onProtectionSettingsSelected() {
        controller.handleTrackingProtectionSettingsSelected()
    }

    override fun onTrackingProtectionShown() {
        controller.handleTrackingProtectionShown()
    }

    override fun onPermissionsShown() {
        controller.handlePermissionsShown()
    }

    override fun onPermissionToggled(permissionState: WebsitePermission) {
        controller.handlePermissionToggled(permissionState)
    }
}
