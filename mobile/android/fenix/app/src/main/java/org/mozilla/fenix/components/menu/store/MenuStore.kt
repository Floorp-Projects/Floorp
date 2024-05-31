/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import androidx.annotation.VisibleForTesting
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [MenuState] and applying [MenuAction]s.
 */
class MenuStore(
    initialState: MenuState,
    middleware: List<Middleware<MenuState, MenuAction>> = listOf(),
) : Store<MenuState, MenuAction>(
    initialState = initialState,
    reducer = ::reducer,
    middleware = middleware,
) {
    init {
        dispatch(MenuAction.InitAction)
    }
}

private fun reducer(state: MenuState, action: MenuAction): MenuState {
    return when (action) {
        is MenuAction.InitAction,
        is MenuAction.AddBookmark,
        is MenuAction.DeleteBrowsingDataAndQuit,
        is MenuAction.Navigate,
        -> state

        is MenuAction.UpdateBookmarkState -> state.copyWithBrowserMenuState {
            it.copy(bookmarkState = action.bookmarkState)
        }
    }
}

@VisibleForTesting
internal inline fun MenuState.copyWithBrowserMenuState(
    crossinline update: (BrowserMenuState) -> BrowserMenuState,
): MenuState {
    return this.copy(browserMenuState = this.browserMenuState?.let { update(it) })
}
