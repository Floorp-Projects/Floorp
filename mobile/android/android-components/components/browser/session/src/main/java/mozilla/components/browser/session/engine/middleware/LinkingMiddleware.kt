/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.engine.EngineObserver
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.ktx.kotlin.isExtensionUrl

/**
 * [Middleware] that handles side-effects of linking a session to an engine session.
 */
internal class LinkingMiddleware(
    private val scope: CoroutineScope,
    private val sessionLookup: (String) -> Session?
) : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        if (action is EngineAction.UnlinkEngineSessionAction) {
            unlink(context, action)
        }

        next(action)

        if (action is EngineAction.LinkEngineSessionAction) {
            link(context, action)
        }
    }

    private fun link(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        action: EngineAction.LinkEngineSessionAction
    ) {
        val tab = context.state.findTabOrCustomTab(action.sessionId) ?: return

        val session = sessionLookup(action.sessionId)
        if (session != null) {
            val observer = EngineObserver(session, context.store)
            action.engineSession.register(observer)
            context.dispatch(EngineAction.UpdateEngineSessionObserverAction(session.id, observer))
        }

        if (action.skipLoading) {
            return
        }

        if (tab.content.url.isExtensionUrl()) {
            // The parent tab/session is used as a referrer which is not accurate
            // for extension pages. The extension page is not loaded by the parent
            // tab, but opened by an extension e.g. via browser.tabs.update.
            performLoadOnMainThread(action.engineSession, tab.content.url)
        } else {
            val parentEngineSession = if (tab is TabSessionState) {
                tab.parentId?.let { context.state.findTabOrCustomTab(it)?.engineState?.engineSession }
            } else {
                null
            }

            performLoadOnMainThread(action.engineSession, tab.content.url, parentEngineSession)
        }
    }

    private fun performLoadOnMainThread(
        engineSession: EngineSession,
        url: String,
        parent: EngineSession? = null
    ) = scope.launch {
        engineSession.loadUrl(url, parent = parent)
    }

    private fun unlink(
        store: MiddlewareContext<BrowserState, BrowserAction>,
        action: EngineAction.UnlinkEngineSessionAction
    ) {
        val tab = store.state.findTabOrCustomTab(action.sessionId) ?: return

        tab.engineState.engineObserver?.let {
            tab.engineState.engineSession?.unregister(it)
        }
    }
}
