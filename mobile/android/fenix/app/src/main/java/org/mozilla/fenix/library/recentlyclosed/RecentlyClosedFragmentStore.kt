/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [RecentlyClosedFragmentState] and applying [RecentlyClosedFragmentAction]s.
 */
class RecentlyClosedFragmentStore(initialState: RecentlyClosedFragmentState) :
    Store<RecentlyClosedFragmentState, RecentlyClosedFragmentAction>(
        initialState,
        ::recentlyClosedStateReducer
    )

/**
 * Actions to dispatch through the `RecentlyClosedFragmentStore` to modify
 * `RecentlyClosedFragmentState` through the reducer.
 */
sealed class RecentlyClosedFragmentAction : Action {
    data class Change(val list: List<RecoverableTab>) : RecentlyClosedFragmentAction()
    data class Select(val tab: RecoverableTab) : RecentlyClosedFragmentAction()
    data class Deselect(val tab: RecoverableTab) : RecentlyClosedFragmentAction()
    object DeselectAll : RecentlyClosedFragmentAction()
}

/**
 * The state for the Recently Closed Screen
 * @property items List of recently closed tabs to display
 */
data class RecentlyClosedFragmentState(
    val items: List<RecoverableTab> = emptyList(),
    val selectedTabs: Set<RecoverableTab>
) : State

/**
 * The RecentlyClosedFragmentState Reducer.
 */
private fun recentlyClosedStateReducer(
    state: RecentlyClosedFragmentState,
    action: RecentlyClosedFragmentAction
): RecentlyClosedFragmentState {
    return when (action) {
        is RecentlyClosedFragmentAction.Change -> state.copy(items = action.list)
        is RecentlyClosedFragmentAction.Select -> {
            state.copy(selectedTabs = state.selectedTabs + action.tab)
        }
        is RecentlyClosedFragmentAction.Deselect -> {
            state.copy(selectedTabs = state.selectedTabs - action.tab)
        }
        RecentlyClosedFragmentAction.DeselectAll -> state.copy(selectedTabs = emptySet())
    }
}
