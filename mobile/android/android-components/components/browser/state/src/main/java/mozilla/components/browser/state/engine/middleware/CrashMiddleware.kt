/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] responsible for recovering crashed [EngineSession] instances.
 */
internal class CrashMiddleware : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        next(action)

        // We need to do this after updating the crashed flag in the reducer
        // because we want observers to see the crash state change before the
        // engine session is cleared. This way the observers can react to
        // crashes and will not request a new engine session until the user
        // explicitly asked to restore the session.
        if (action is CrashAction.SessionCrashedAction) {
            onCrash(context, action)
        }
    }

    private fun onCrash(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        action: CrashAction.SessionCrashedAction,
    ) {
        // We suspend the crashed session here. After that the reducer will mark it as "crashed".
        // That will prevent it from getting recreated until explicitly handling the crash by
        // restoring.
        context.dispatch(
            EngineAction.SuspendEngineSessionAction(action.tabId),
        )
    }
}
