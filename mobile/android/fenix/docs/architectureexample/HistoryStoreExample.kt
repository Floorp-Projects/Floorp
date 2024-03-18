/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class HistoryStore(
    private val initialState: HistoryState,
    private val middleware: List<Middleware<HistoryState, HistoryAction>>
) : Store<HistoryState, Reducer<HistoryState, HistoryAction>>(initialState, middleware, ::reducer) {
    init {
        // This will ensure that middlewares can take any actions they need to during initialization
        dispatch(HistoryAction.Init)
    }
}

sealed class HistoryAction {
    object Init : HistoryAction()
    data class ItemsChanged(val items: List<History>) : HistoryAction()
    data class DeleteItems(val items: List<History>) : HistoryAction()
    data class DeleteFinished() : HistoryAction()
    data class ToggleItemSelection(val item: History) : HistoryAction()
    data class OpenItem(val item: History) : HistoryAction()
}

data class HistoryState(
    val items: List<History>,
    val selectedItems: List<History>,
    val itemsBeingDeleted: List<History>,
    companion object {
        val initial = HistoryState(
            items = listOf(),
            selectedItems = listOf(),
            itemsBeingDeleted = listOf(),
        )
    }
) {
    val displayItems = items.filter { item ->
        item !in itemsBeingDeleted
    }
}

fun reducer(oldState: HistoryState, action: HistoryAction): HistoryState = when (action) {
    is HistoryAction.ItemsChanged -> oldState.copy(items = action.items)
    is HistoryAction.DeleteItems -> oldState.copy(itemsBeingDeleted = action.items)
    is HistoryAction.DeleteFinished -> oldState.copy(
        items = oldState.items - oldState.itemsBeingDeleted,
        itemsBeingDeleted = listOf(),
    )
    is HistoryAction.ToggleItemSelection -> {
        if (oldState.selectedItems.contains(action.item)) {
            oldState.copy(selectedItems = oldState.selectedItems - action.item)
        } else {
            oldState.copy(selectedItems = oldState.selectedItems + action.item)
        }
    }
    else -> Unit
}

data class History(
    val id: Int,
    val title: String,
    val url: Uri,
)
