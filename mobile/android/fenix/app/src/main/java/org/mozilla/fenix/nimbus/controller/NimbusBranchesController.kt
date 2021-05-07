/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus.controller

import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.service.nimbus.ui.NimbusBranchesAdapterDelegate
import org.mozilla.experiments.nimbus.Branch
import org.mozilla.fenix.nimbus.NimbusBranchesAction
import org.mozilla.fenix.nimbus.NimbusBranchesStore

/**
 * [NimbusBranchesFragment] controller. This implements [NimbusBranchesAdapterDelegate] to handle
 * interactions with a Nimbus branch item.
 *
 * @param nimbusBranchesStore An instance of [NimbusBranchesStore] for dispatching
 * [NimbusBranchesAction]s.
 * @param experiments An instance of [NimbusApi] for interacting with the Nimbus experiments.
 * @param experimentId The string experiment-id or "slug" for a Nimbus experiment.
 */
class NimbusBranchesController(
    private val nimbusBranchesStore: NimbusBranchesStore,
    private val experiments: NimbusApi,
    private val experimentId: String
) : NimbusBranchesAdapterDelegate {

    override fun onBranchItemClicked(branch: Branch) {
        experiments.optInWithBranch(experimentId, branch.slug)
        nimbusBranchesStore.dispatch(NimbusBranchesAction.UpdateSelectedBranch(branch.slug))
    }
}
