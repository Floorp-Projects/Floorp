/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineObserver
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.ktx.kotlin.isExtensionUrl
import java.lang.IllegalArgumentException

/**
 * [Middleware] that handles side-effects of linking a session to an engine session.
 */
internal class LinkingMiddleware(
    private val scope: CoroutineScope,
) : Middleware<BrowserState, BrowserAction> {

    @Suppress("ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        var engineObserver: Pair<String, EngineObserver>? = null
        when (action) {
            is TabListAction.AddTabAction -> {
                if (action.tab.engineState.engineSession != null && action.tab.engineState.engineObserver == null) {
                    engineObserver = link(context, action.tab.engineState.engineSession, action.tab)
                }
            }
            is TabListAction.AddMultipleTabsAction -> {
                if (action.tabs.any { it.engineState.engineSession != null }) {
                    throw IllegalArgumentException("AddMultipleTabsAction does not support tabs with engine sessions")
                }
            }
            is EngineAction.UnlinkEngineSessionAction -> {
                unlink(context, action)
            }
            else -> {
                // no-op
            }
        }

        next(action)

        when (action) {
            is EngineAction.LinkEngineSessionAction -> {
                context.state.findTabOrCustomTab(action.tabId)?.let { tab ->
                    engineObserver = link(context, action.engineSession, tab, action.skipLoading)
                }
            }
            else -> {
                // no-op
            }
        }

        engineObserver?.let {
            context.dispatch(EngineAction.UpdateEngineSessionObserverAction(it.first, it.second))
            context.dispatch(EngineAction.UpdateEngineSessionInitializingAction(it.first, false))
        }
    }

    private fun link(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        engineSession: EngineSession,
        tab: SessionState,
        skipLoading: Boolean = true,
    ): Pair<String, EngineObserver> {
        val observer = EngineObserver(tab.id, context.store)
        engineSession.register(observer)

        if (skipLoading) {
            return Pair(tab.id, observer)
        }

        if (tab.content.url.isExtensionUrl()) {
            // The parent tab/session is used as a referrer which is not accurate
            // for extension pages. The extension page is not loaded by the parent
            // tab, but opened by an extension e.g. via browser.tabs.update.
            performLoadOnMainThread(engineSession, tab.content.url, loadFlags = tab.engineState.initialLoadFlags)
        } else {
            val parentEngineSession = if (tab is TabSessionState) {
                tab.parentId?.let { context.state.findTabOrCustomTab(it)?.engineState?.engineSession }
            } else {
                null
            }

            performLoadOnMainThread(
                engineSession,
                tab.content.url,
                parentEngineSession,
                loadFlags = tab.engineState.initialLoadFlags,
            )
        }

        return Pair(tab.id, observer)
    }

    private fun performLoadOnMainThread(
        engineSession: EngineSession,
        url: String,
        parent: EngineSession? = null,
        loadFlags: EngineSession.LoadUrlFlags,
    ) = scope.launch {
        engineSession.loadUrl(url, parent = parent, flags = loadFlags)
    }

    private fun unlink(
        store: MiddlewareContext<BrowserState, BrowserAction>,
        action: EngineAction.UnlinkEngineSessionAction,
    ) {
        val tab = store.state.findTabOrCustomTab(action.tabId) ?: return

        tab.engineState.engineObserver?.let {
            tab.engineState.engineSession?.unregister(it)
        }
    }
}
