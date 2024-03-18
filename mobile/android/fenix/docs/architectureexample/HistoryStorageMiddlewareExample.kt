/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class HistoryStorageMiddleware(
    private val storage: HistoryStorage
    private val scope: CoroutineScope,
) : Middleware<HistoryState, HistoryAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryState, HistoryAction>,
        next: (HistoryAction) -> Unit,
        action: HistoryAction,
    ) {
        // This middleware won't need to manipulate the action, so the action can be passed through
        // the middleware chain before the side-effects are initiated
        next(action)
        when(action) {
            is HistoryAction.Init -> {
                scope.launch {
                    val history = storage.load()
                    context.store.dispatch(HistoryAction.ItemsChanged(history))
                }
            }
            is HistoryAction.DeleteItems -> {
                scope.launch {
                    val currentItems = context.state.items
                    if (storage.delete(action.items) is HistoryStorage.Success) {
                        context.store.dispatch(
                            HistoryAction.DeleteFinished()
                        )
                    }
                }
            }
            else -> Unit
        }
    }
}
