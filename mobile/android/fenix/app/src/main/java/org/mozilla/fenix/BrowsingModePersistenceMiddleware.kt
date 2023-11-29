/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.utils.Settings

/**
 * Middleware for controlling side-effects relating to Private Browsing Mode.
 *
 * @param settings Used to update disk-cache related PBM data.
 * @param scope Scope used for disk writes and reads. Exposed for test overrides.
 */
class BrowsingModePersistenceMiddleware(
    private val settings: Settings,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<AppState, AppAction> {
    override fun invoke(
        context: MiddlewareContext<AppState, AppAction>,
        next: (AppAction) -> Unit,
        action: AppAction,
    ) {
        val initialMode = context.state.mode
        next(action)
        val updatedMode = context.state.mode
        if (initialMode != context.state.mode) {
            scope.launch {
                settings.lastKnownMode = updatedMode
            }
        }
        when (action) {
            is AppAction.Init -> {
                scope.launch {
                    val mode = settings.lastKnownMode
                    context.store.dispatch(AppAction.BrowsingModeLoaded(mode))
                }
            }
            else -> Unit
        }
    }
}
