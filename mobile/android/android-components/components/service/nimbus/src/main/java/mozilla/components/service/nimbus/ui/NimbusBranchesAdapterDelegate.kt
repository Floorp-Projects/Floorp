/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import org.mozilla.experiments.nimbus.Branch

/**
 * Provides method for handling the branch items in the Nimbus branches view.
 */
interface NimbusBranchesAdapterDelegate {
    /**
     * Handler for when a branch item is clicked.
     *
     * @param branch The [Branch] that was clicked.
     */
    fun onBranchItemClicked(branch: Branch) = Unit
}
