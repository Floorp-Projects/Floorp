/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

/**
 * [ConnectionPanelDialogFragment] interactor.
 *
 * Implements callbacks for each of [ConnectionPanelDialogFragment]'s Views declared possible user interactions,
 * delegates all such user events to the [ConnectionDetailsController].
 *
 * @param controller [ConnectionDetailsController] which will be delegated for all users interactions,
 * it expected to contain all business logic for how to act in response.
 */
class ConnectionDetailsInteractor(
    private val controller: ConnectionDetailsController
) : WebSiteInfoInteractor {

    override fun onBackPressed() {
        controller.handleBackPressed()
    }
}
