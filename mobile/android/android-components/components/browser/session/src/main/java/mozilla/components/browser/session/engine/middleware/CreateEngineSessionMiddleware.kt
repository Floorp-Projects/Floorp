/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.engine.getOrCreateEngineSession
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.support.base.log.logger.Logger

/**
 * [Middleware] responsible for creating [EngineSession] instances whenever an [EngineAction.CreateEngineSessionAction]
 * is getting dispatched.
 */
internal class CreateEngineSessionMiddleware(
    private val engine: Engine,
    private val sessionLookup: (String) -> Session?,
    private val scope: CoroutineScope
) : Middleware<BrowserState, BrowserAction> {
    private val logger = Logger("CreateEngineSessionMiddleware")

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        if (action is EngineAction.CreateEngineSessionAction) {
            createEngineSession(context.store, action)
        } else {
            next(action)
        }
    }

    private fun createEngineSession(
        store: Store<BrowserState, BrowserAction>,
        action: EngineAction.CreateEngineSessionAction
    ) {
        logger.debug("Request to create engine session for tab ${action.tabId}")

        scope.launch {
            // We only need to ask for an EngineSession here. If needed this method will internally
            // create one and dispatch a LinkEngineSessionAction to add it to BrowserState.
            getOrCreateEngineSession(
                engine,
                logger,
                sessionLookup,
                store,
                action.tabId
            )
        }
    }
}
