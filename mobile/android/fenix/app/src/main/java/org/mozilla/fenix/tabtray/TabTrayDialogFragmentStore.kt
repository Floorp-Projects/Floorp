/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.tabstray.Tab
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [TabTrayDialogFragmentState] and
 * applying [TabTrayDialogFragmentAction]s.
 */
class TabTrayDialogFragmentStore(initialState: TabTrayDialogFragmentState) :
    Store<TabTrayDialogFragmentState, TabTrayDialogFragmentAction>(
        initialState,
        ::tabTrayStateReducer
    )

/**
 * Actions to dispatch through the `TabTrayDialogFragmentStore` to modify
 * `TabTrayDialogFragmentState` through the reducer.
 */
sealed class TabTrayDialogFragmentAction : Action {
    data class BrowserStateChanged(val browserState: BrowserState) : TabTrayDialogFragmentAction()
    object EnterMultiSelectMode : TabTrayDialogFragmentAction()
    object ExitMultiSelectMode : TabTrayDialogFragmentAction()
    data class AddItemForCollection(val item: Tab) : TabTrayDialogFragmentAction()
    data class RemoveItemForCollection(val item: Tab) : TabTrayDialogFragmentAction()
}

/**
 * The state for the Tab Tray Dialog Screen
 * @property mode Current Mode of Multiselection
 */
data class TabTrayDialogFragmentState(val browserState: BrowserState, val mode: Mode) : State {
    sealed class Mode {
        open val selectedItems = emptySet<Tab>()

        object Normal : Mode()
        data class MultiSelect(override val selectedItems: Set<Tab>) : Mode()
    }
}

/**
 * The TabTrayDialogFragmentState Reducer.
 */
private fun tabTrayStateReducer(
    state: TabTrayDialogFragmentState,
    action: TabTrayDialogFragmentAction
): TabTrayDialogFragmentState {
    return when (action) {
        is TabTrayDialogFragmentAction.BrowserStateChanged -> state.copy(browserState = action.browserState)
        is TabTrayDialogFragmentAction.AddItemForCollection ->
            state.copy(mode = TabTrayDialogFragmentState.Mode.MultiSelect(state.mode.selectedItems + action.item))
        is TabTrayDialogFragmentAction.RemoveItemForCollection -> {
            val selected = state.mode.selectedItems - action.item
            state.copy(
                mode = if (selected.isEmpty()) {
                    TabTrayDialogFragmentState.Mode.Normal
                } else {
                    TabTrayDialogFragmentState.Mode.MultiSelect(selected)
                }
            )
        }
        is TabTrayDialogFragmentAction.ExitMultiSelectMode -> state.copy(mode = TabTrayDialogFragmentState.Mode.Normal)
        is TabTrayDialogFragmentAction.EnterMultiSelectMode -> state.copy(
            mode = TabTrayDialogFragmentState.Mode.MultiSelect(
                setOf()
            )
        )
    }
}
