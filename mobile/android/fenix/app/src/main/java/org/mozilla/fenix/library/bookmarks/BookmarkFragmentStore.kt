/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

class BookmarkFragmentStore(
    initalState: BookmarkFragmentState
) : Store<BookmarkFragmentState, BookmarkFragmentAction>(
    initalState, ::bookmarkFragmentStateReducer
)

/**
 * The complete state of the bookmarks tree and multi-selection mode
 * @property tree The current tree of bookmarks, if one is loaded
 * @property mode The current bookmark multi-selection mode
 */
data class BookmarkFragmentState(
    val tree: BookmarkNode?,
    val mode: Mode = Mode.Normal,
    val isLoading: Boolean = true
) : State {
    sealed class Mode {
        open val selectedItems = emptySet<BookmarkNode>()

        object Normal : Mode()
        data class Selecting(override val selectedItems: Set<BookmarkNode>) : Mode()
    }
}

/**
 * Actions to dispatch through the `BookmarkStore` to modify `BookmarkState` through the reducer.
 */
sealed class BookmarkFragmentAction : Action {
    data class Change(val tree: BookmarkNode) : BookmarkFragmentAction()
    data class Select(val item: BookmarkNode) : BookmarkFragmentAction()
    data class Deselect(val item: BookmarkNode) : BookmarkFragmentAction()
    object DeselectAll : BookmarkFragmentAction()
}

/**
 * Reduces the bookmarks state from the current state and an action performed on it.
 * @param state the current bookmarks state
 * @param action the action to perform
 * @return the new bookmarks state
 */
private fun bookmarkFragmentStateReducer(
    state: BookmarkFragmentState,
    action: BookmarkFragmentAction
): BookmarkFragmentState {
    return when (action) {
        is BookmarkFragmentAction.Change -> {
            val items = state.mode.selectedItems.filter { it in action.tree }
            state.copy(
                tree = action.tree,
                mode = if (items.isEmpty()) {
                    BookmarkFragmentState.Mode.Normal
                } else {
                    BookmarkFragmentState.Mode.Selecting(items.toSet())
                },
                isLoading = false
            )
        }
        is BookmarkFragmentAction.Select ->
            state.copy(mode = BookmarkFragmentState.Mode.Selecting(state.mode.selectedItems + action.item))
        is BookmarkFragmentAction.Deselect -> {
            val items = state.mode.selectedItems - action.item
            state.copy(
                mode = if (items.isEmpty()) {
                    BookmarkFragmentState.Mode.Normal
                } else {
                    BookmarkFragmentState.Mode.Selecting(items)
                }
            )
        }
        BookmarkFragmentAction.DeselectAll ->
            state.copy(mode = BookmarkFragmentState.Mode.Normal)
    }
}

operator fun BookmarkNode.contains(item: BookmarkNode): Boolean {
    return children?.contains(item) ?: false
}
