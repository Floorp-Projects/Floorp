/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class HistoryTelemetryMiddleware : Middleware<HistoryState, HistoryAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryState, HistoryAction>,
        next: (HistoryAction) -> Unit,
        action: HistoryAction,
    ) {
        // This middleware won't need to manipulate the action, so the action can be passed through
        // the middleware chain before the side-effects are initiated
        next(action)
        when(action) {
            is HistoryAction.DeleteItems -> History.itemsDeleted.record()
            is HistoryAction.OpenItem -> History.itemOpened.record()
            else -> Unit
        }
    }
}
