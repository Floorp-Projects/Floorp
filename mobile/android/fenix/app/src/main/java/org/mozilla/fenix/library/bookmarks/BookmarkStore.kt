/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

class BookmarkStore(
    initalState: BookmarkState
) : Store<BookmarkState, BookmarkAction>(
    initalState, ::bookmarkStateReducer
)

/**
 * The complete state of the bookmarks tree and multi-selection mode
 * @property tree The current tree of bookmarks, if one is loaded
 * @property mode The current bookmark multi-selection mode
 */
data class BookmarkState(val tree: BookmarkNode?, val mode: Mode = Mode.Normal) : State {
    sealed class Mode {
        object Normal : Mode()
        data class Selecting(val selectedItems: Set<BookmarkNode>) : Mode()
    }
}

/**
 * Actions to dispatch through the `BookmarkStore` to modify `BookmarkState` through the reducer.
 */
sealed class BookmarkAction : Action {
    data class Change(val tree: BookmarkNode) : BookmarkAction()
    data class Select(val item: BookmarkNode) : BookmarkAction()
    data class Deselect(val item: BookmarkNode) : BookmarkAction()
    object DeselectAll : BookmarkAction()
}

/**
 * Reduces the bookmarks state from the current state and an action performed on it.
 * @param state the current bookmarks state
 * @param action the action to perform
 * @return the new bookmarks state
 */
fun bookmarkStateReducer(state: BookmarkState, action: BookmarkAction): BookmarkState {
    return when (action) {
        is BookmarkAction.Change -> {
            val mode =
                if (state.mode is BookmarkState.Mode.Selecting) {
                    val items = state.mode.selectedItems.filter {
                        it in action.tree
                    }.toSet()
                    if (items.isEmpty()) BookmarkState.Mode.Normal else BookmarkState.Mode.Selecting(items)
                } else state.mode
            state.copy(tree = action.tree, mode = mode)
        }
        is BookmarkAction.Select -> {
            val selectedItems = if (state.mode is BookmarkState.Mode.Selecting) {
                state.mode.selectedItems + action.item
            } else setOf(action.item)
            state.copy(mode = BookmarkState.Mode.Selecting(selectedItems))
        }
        is BookmarkAction.Deselect -> {
            val selectedItems = if (state.mode is BookmarkState.Mode.Selecting) {
                state.mode.selectedItems - action.item
            } else setOf()
            val mode =
                if (selectedItems.isEmpty()) BookmarkState.Mode.Normal else BookmarkState.Mode.Selecting(selectedItems)
            state.copy(mode = mode)
        }
        BookmarkAction.DeselectAll -> {
            state.copy(mode = BookmarkState.Mode.Normal)
        }
    }
}

operator fun BookmarkNode.contains(item: BookmarkNode): Boolean {
    return children?.contains(item) ?: false
}
