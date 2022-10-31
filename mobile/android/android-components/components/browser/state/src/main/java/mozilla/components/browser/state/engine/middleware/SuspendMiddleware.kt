/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] implementation responsible for suspending an [EngineSession].
 *
 * Suspending an [EngineSession] means that we will take the last [EngineSessionState], attach that
 * to [EngineState] and then clear the [EngineSession] reference and close it. The next time we
 * need an [EngineSession] for this tab we will create a new instance and restore the attached
 * [EngineSessionState].
 */
internal class SuspendMiddleware(
    private val scope: CoroutineScope,
) : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is EngineAction.SuspendEngineSessionAction -> suspend(context, action.tabId)
            is EngineAction.KillEngineSessionAction -> suspend(context, action.tabId)
            else -> next(action)
        }
    }

    private fun suspend(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        sessionId: String,
    ) {
        val tab = context.state.findTab(sessionId) ?: return

        // First we unlink (which clearsEngineSession and state)
        context.dispatch(
            EngineAction.UnlinkEngineSessionAction(
                tab.id,
            ),
        )

        // Now we can close the unlinked EngineSession (on the main thread).
        scope.launch {
            tab.engineState.engineSession?.close()
        }
    }
}
