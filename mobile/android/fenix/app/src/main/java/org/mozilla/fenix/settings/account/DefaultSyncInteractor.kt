/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

interface SyncInteractor {
    fun onCameraPermissionsNeeded()
}

/**
 * Interactor for [TurnOnSyncFragment].
 *
 * @param syncController Handles the interactions
 */
class DefaultSyncInteractor(private val syncController: DefaultSyncController) : SyncInteractor {
    override fun onCameraPermissionsNeeded() {
        syncController.handleCameraPermissionsNeeded()
    }
}
