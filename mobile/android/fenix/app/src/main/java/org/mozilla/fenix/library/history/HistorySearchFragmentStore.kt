/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [HistorySearchFragmentState] and applying [HistorySearchFragmentAction]s.
 */
class HistorySearchFragmentStore(
    initialState: HistorySearchFragmentState
) : Store<HistorySearchFragmentState, HistorySearchFragmentAction>(
    initialState,
    ::historySearchStateReducer
)

/**
 * The state for the History Search Screen
 *
 * @property query The current search query string
 */
data class HistorySearchFragmentState(
    val query: String,
) : State

fun createInitialHistorySearchFragmentState(): HistorySearchFragmentState {
    return HistorySearchFragmentState(query = "")
}

/**
 * Actions to dispatch through the [HistorySearchFragmentStore] to modify [HistorySearchFragmentState]
 * through the reducer.
 */
sealed class HistorySearchFragmentAction : Action {
    data class UpdateQuery(val query: String) : HistorySearchFragmentAction()
}

/**
 * The [HistorySearchFragmentState] Reducer.
 */
private fun historySearchStateReducer(
    state: HistorySearchFragmentState,
    action: HistorySearchFragmentAction
): HistorySearchFragmentState {
    return when (action) {
        is HistorySearchFragmentAction.UpdateQuery ->
            state.copy(query = action.query)
    }
}
