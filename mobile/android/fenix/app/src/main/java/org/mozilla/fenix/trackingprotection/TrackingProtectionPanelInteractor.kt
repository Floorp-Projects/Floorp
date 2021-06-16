/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

/**
 * Interactor for the tracking protection panel
 * Provides implementations for the TrackingProtectionPanelViewInteractor
 */
class TrackingProtectionPanelInteractor(
    private val store: TrackingProtectionStore,
    private val openTrackingProtectionSettings: () -> Unit
) : TrackingProtectionPanelViewInteractor {
    override fun openDetails(category: TrackingProtectionCategory, categoryBlocked: Boolean) {
        store.dispatch(TrackingProtectionAction.EnterDetailsMode(category, categoryBlocked))
    }

    override fun selectTrackingProtectionSettings() {
        openTrackingProtectionSettings.invoke()
    }

    override fun onBackPressed() {
        store.dispatch(TrackingProtectionAction.ExitDetailsMode)
    }
}
