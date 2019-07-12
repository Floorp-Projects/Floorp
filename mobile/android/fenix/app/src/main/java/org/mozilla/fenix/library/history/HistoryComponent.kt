/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.ViewGroup
import org.mozilla.fenix.mvi.ViewState
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.test.Mockable

data class HistoryItem(val id: Int, val title: String, val url: String, val visitedAt: Long)

@Mockable
class HistoryComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    viewModelProvider: UIComponentViewModelProvider<HistoryState, HistoryChange>
) :
    UIComponent<HistoryState, HistoryAction, HistoryChange>(
        bus.getManagedEmitter(HistoryAction::class.java),
        bus.getSafeManagedObservable(HistoryChange::class.java),
        viewModelProvider
    ) {

    override fun initView() = HistoryUIView(container, actionEmitter, changesObservable)

    init {
        bind()
    }
}

data class HistoryState(val items: List<HistoryItem>, val mode: Mode) : ViewState {
    sealed class Mode {
        object Normal : Mode()
        data class Editing(val selectedItems: List<HistoryItem>) : Mode()
        object Deleting : Mode()
    }
}

sealed class HistoryAction : Action {
    data class Open(val item: HistoryItem) : HistoryAction()
    data class EnterEditMode(val item: HistoryItem) : HistoryAction()
    object BackPressed : HistoryAction()
    data class AddItemForRemoval(val item: HistoryItem) : HistoryAction()
    data class RemoveItemForRemoval(val item: HistoryItem) : HistoryAction()
    object SwitchMode : HistoryAction()

    sealed class Delete : HistoryAction() {
        object All : Delete()
        data class One(val item: HistoryItem) : Delete()
        data class Some(val items: List<HistoryItem>) : Delete()
    }
}

sealed class HistoryChange : Change {
    data class Change(val list: List<HistoryItem>) : HistoryChange()
    data class EnterEditMode(val item: HistoryItem) : HistoryChange()
    object ExitEditMode : HistoryChange()
    data class AddItemForRemoval(val item: HistoryItem) : HistoryChange()
    data class RemoveItemForRemoval(val item: HistoryItem) : HistoryChange()
    object EnterDeletionMode : HistoryChange()
    object ExitDeletionMode : HistoryChange()
}

class HistoryViewModel(
    initialState: HistoryState
) : UIComponentViewModelBase<HistoryState, HistoryChange>(initialState, reducer) {
    companion object {
        fun create() = HistoryViewModel(HistoryState(emptyList(), HistoryState.Mode.Normal))
        val reducer: (HistoryState, HistoryChange) -> HistoryState = { state, change ->
            when (change) {
                is HistoryChange.Change -> state.copy(mode = HistoryState.Mode.Normal, items = change.list)
                is HistoryChange.EnterEditMode -> state.copy(mode = HistoryState.Mode.Editing(listOf(change.item)))
                is HistoryChange.AddItemForRemoval -> {
                    val mode = state.mode
                    if (mode is HistoryState.Mode.Editing) {
                        val items = mode.selectedItems + listOf(change.item)
                        state.copy(mode = mode.copy(selectedItems = items))
                    } else {
                        state
                    }
                }
                is HistoryChange.RemoveItemForRemoval -> {
                    var mode = state.mode

                    if (mode is HistoryState.Mode.Editing) {
                        val items = mode.selectedItems.filter { it.id != change.item.id }
                        mode = if (items.isEmpty()) HistoryState.Mode.Normal else HistoryState.Mode.Editing(items)

                        state.copy(mode = mode)
                    } else {
                        state
                    }
                }
                is HistoryChange.ExitEditMode -> state.copy(mode = HistoryState.Mode.Normal)
                is HistoryChange.EnterDeletionMode -> state.copy(mode = HistoryState.Mode.Deleting)
                is HistoryChange.ExitDeletionMode -> state.copy(mode = HistoryState.Mode.Normal)
            }
        }
    }
}
