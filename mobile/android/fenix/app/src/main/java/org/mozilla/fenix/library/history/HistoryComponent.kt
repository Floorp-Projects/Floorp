/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.library.history

import android.view.ViewGroup
import org.mozilla.fenix.test.Mockable
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState
import java.net.URL

data class HistoryItem(val id: Int, val url: String, val visitedAt: Long) {
    val title: String
        get() = siteTitle()

    @SuppressWarnings("TooGenericExceptionCaught")
    private fun siteTitle(): String {
        return try {
            URL(url).host
        } catch (e: Exception) {
            url
        }
    }
}

@Mockable
class HistoryComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: HistoryState = HistoryState(emptyList(), HistoryState.Mode.Normal)
) :
    UIComponent<HistoryState, HistoryAction, HistoryChange>(
        bus.getManagedEmitter(HistoryAction::class.java),
        bus.getSafeManagedObservable(HistoryChange::class.java)
    ) {

    override val reducer: (HistoryState, HistoryChange) -> HistoryState = { state, change ->
        when (change) {
            is HistoryChange.Change -> state.copy(items = change.list)
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
                val mode = state.mode
                if (mode is HistoryState.Mode.Editing) {
                    val items = mode.selectedItems.filter { it.id != change.item.id }
                    state.copy(mode = mode.copy(selectedItems = items))
                } else {
                    state
                }
            }
            is HistoryChange.ExitEditMode -> state.copy(mode = HistoryState.Mode.Normal)
        }
    }

    override fun initView() = HistoryUIView(container, actionEmitter, changesObservable)

    init {
        render(reducer)
    }
}

data class HistoryState(val items: List<HistoryItem>, val mode: Mode) : ViewState {
    sealed class Mode {
        object Normal : Mode()
        data class Editing(val selectedItems: List<HistoryItem>) : Mode()
    }
}

sealed class HistoryAction : Action {
    data class Select(val item: HistoryItem) : HistoryAction()
    data class EnterEditMode(val item: HistoryItem) : HistoryAction()
    object BackPressed : HistoryAction()
    data class AddItemForRemoval(val item: HistoryItem) : HistoryAction()
    data class RemoveItemForRemoval(val item: HistoryItem) : HistoryAction()

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
}
