/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.middleware

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] that handles updating the [TabSessionState.lastAccess] when a tab is selected.
 */
class LastAccessMiddleware : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        next(action)

        when (action) {
            is TabListAction.SelectTabAction -> {
                context.dispatchUpdateActionForId(action.tabId)
            }
            is TabListAction.AddTabAction -> {
                if (action.select) {
                    context.dispatchUpdateActionForId(action.tab.id)
                }
            }
            is ContentAction.UpdateUrlAction -> {
                if (action.sessionId == context.state.selectedTabId) {
                    context.dispatchUpdateActionForId(action.sessionId)
                }
            }
        }
    }

    private fun MiddlewareContext<BrowserState, BrowserAction>.dispatchUpdateActionForId(id: String) {
        dispatch(LastAccessAction.UpdateLastAccessAction(id, System.currentTimeMillis()))
    }
}
