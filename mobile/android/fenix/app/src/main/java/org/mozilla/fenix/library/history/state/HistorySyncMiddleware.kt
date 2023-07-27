/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.SyncReason
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState

/**
 * A [Middleware] for handling Sync side-effects based on [HistoryFragmentAction]s that are dispatched
 * to the [HistoryFragmentStore].
 *
 * @property accountManager The [FxaAccountManager] for handling Sync operations.
 * @property refreshView Callback to refresh the view once a Sync is completed.
 * @property scope Coroutine scope to launch Sync operations into.
 */
class HistorySyncMiddleware(
    private val accountManager: FxaAccountManager,
    private val refreshView: () -> Unit,
    private val scope: CoroutineScope,
) : Middleware<HistoryFragmentState, HistoryFragmentAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryFragmentState, HistoryFragmentAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        next(action)
        when (action) {
            is HistoryFragmentAction.StartSync -> {
                scope.launch {
                    accountManager.syncNow(
                        reason = SyncReason.User,
                        debounce = true,
                        customEngineSubset = listOf(SyncEngine.History),
                    )
                    refreshView()
                    context.store.dispatch(HistoryFragmentAction.FinishSync)
                }
            }
            else -> Unit
        }
    }
}
