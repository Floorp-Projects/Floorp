/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [BookmarkSearchFragmentState] and applying [BookmarkSearchFragmentAction]s.
 */
class BookmarkSearchFragmentStore(
    initialState: BookmarkSearchFragmentState
) : Store<BookmarkSearchFragmentState, BookmarkSearchFragmentAction>(
    initialState,
    ::bookmarkSearchStateReducer
)

/**
 * The state for the Bookmark Search Screen
 *
 * @property query The current search query string
 */
data class BookmarkSearchFragmentState(
    val query: String,
) : State

fun createInitialBookmarkSearchFragmentState(): BookmarkSearchFragmentState {
    return BookmarkSearchFragmentState(query = "")
}

/**
 * Actions to dispatch through the [BookmarkSearchFragmentStore] to modify [BookmarkSearchFragmentState]
 * through the reducer.
 */
sealed class BookmarkSearchFragmentAction : Action {
    data class UpdateQuery(val query: String) : BookmarkSearchFragmentAction()
}

/**
 * The [BookmarkSearchFragmentState] Reducer.
 */
private fun bookmarkSearchStateReducer(
    state: BookmarkSearchFragmentState,
    action: BookmarkSearchFragmentAction
): BookmarkSearchFragmentState {
    return when (action) {
        is BookmarkSearchFragmentAction.UpdateQuery ->
            state.copy(query = action.query)
    }
}
