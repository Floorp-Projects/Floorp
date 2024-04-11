/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [MenuState] and applying [MenuAction]s.
 */
class MenuStore(
    initialState: MenuState,
    middleware: List<Middleware<MenuState, MenuAction>> = listOf(),
) :
    Store<MenuState, MenuAction>(
        initialState = initialState,
        reducer = ::reducer,
        middleware = middleware,
    )

private fun reducer(state: MenuState, action: MenuAction): MenuState {
    return when (action) {
        is MenuAction.UpdateBookmarked -> state.copy(isBookmarked = action.isBookmarked)
        is MenuAction.Navigate -> state
    }
}
