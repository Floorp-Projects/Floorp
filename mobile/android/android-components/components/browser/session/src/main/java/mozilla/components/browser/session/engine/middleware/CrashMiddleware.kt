/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.engine.getOrCreateEngineSession
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.support.base.log.logger.Logger

/**
 * [Middleware] responsible for recovering crashed [EngineSession] instances.
 */
internal class CrashMiddleware(
    private val engine: Engine,
    private val sessionLookup: (String) -> Session?,
    private val scope: CoroutineScope
) : Middleware<BrowserState, BrowserAction> {
    private val logger = Logger("CrashMiddleware")

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        if (action is CrashAction.RestoreCrashedSessionAction) {
            restore(context.store, action)
        }

        next(action)
    }

    private fun restore(
        store: Store<BrowserState, BrowserAction>,
        action: CrashAction.RestoreCrashedSessionAction
    ) = scope.launch {
        val tab = store.state.findTabOrCustomTab(action.tabId) ?: return@launch

        // Currently we are forcing the creation of an engine session here. This is mimicing
        // the previous behavior. But it is questionable if this is the right approach:
        // - How did this tab crash if it does not have an engine session?
        // - Instead of creating the engine session, could we turn it into a suspended
        //   session with the "crash state" as the last state?
        val engineSession = getOrCreateEngineSession(engine, logger, sessionLookup, store, tab.id)
        engineSession?.recoverFromCrash()
    }
}
