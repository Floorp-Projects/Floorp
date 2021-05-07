/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.experiments.nimbus.Branch

/**
 * The [Store] for holding the [NimbusBranchesState] and applying [NimbusBranchesAction]s.
 */
class NimbusBranchesStore(initialState: NimbusBranchesState) :
    Store<NimbusBranchesState, NimbusBranchesAction>(
        initialState, ::nimbusBranchesFragmentStateReducer
    )

/**
 * The state for [NimbusBranchesFragment].
 *
 * @property branches The list of [Branch]s to display in the branches list.
 * @property selectedBranch The selected [Branch] slug for a Nimbus experiment.
 * @property isLoading True if the branches are still being loaded from storage, otherwise false.
 */
data class NimbusBranchesState(
    val branches: List<Branch>,
    val selectedBranch: String = "",
    val isLoading: Boolean = true
) : State

/**
 * Actions to dispatch through the [NimbusBranchesStore] to modify the [NimbusBranchesState]
 * through the [nimbusBranchesFragmentStateReducer].
 */
sealed class NimbusBranchesAction : Action {
    /**
     * Updates the list of Nimbus branches and selected branch.
     *
     * @param branches The list of [Branch]s to display in the branches list.
     * @param selectedBranch The selected [Branch] slug for a Nimbus experiment.
     */
    data class UpdateBranches(val branches: List<Branch>, val selectedBranch: String) :
        NimbusBranchesAction()

    /**
     * Updates the selected branch.
     *
     * @param selectedBranch The selected [Branch] slug for a Nimbus experiment.
     */
    data class UpdateSelectedBranch(val selectedBranch: String) : NimbusBranchesAction()
}

/**
 * Reduces the Nimbus branches state from the current state with the provided [action] to
 * be performed.
 *
 * @param state The current Nimbus branches state.
 * @param action The action to be performed on the state.
 * @return the new [NimbusBranchesState] with the [action] executed.
 */
private fun nimbusBranchesFragmentStateReducer(
    state: NimbusBranchesState,
    action: NimbusBranchesAction
): NimbusBranchesState {
    return when (action) {
        is NimbusBranchesAction.UpdateBranches -> {
            state.copy(
                branches = action.branches,
                selectedBranch = action.selectedBranch,
                isLoading = false
            )
        }
        is NimbusBranchesAction.UpdateSelectedBranch -> {
            state.copy(selectedBranch = action.selectedBranch)
        }
    }
}
