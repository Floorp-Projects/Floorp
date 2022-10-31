/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import androidx.annotation.MainThread
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
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
    private val scope: CoroutineScope,
) : Middleware<BrowserState, BrowserAction> {
    private val logger = Logger("CreateEngineSessionMiddleware")

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        if (action is EngineAction.CreateEngineSessionAction) {
            if (context.state.findTabOrCustomTab(action.tabId)?.engineState?.initializing == false) {
                context.dispatch(EngineAction.UpdateEngineSessionInitializingAction(action.tabId, true))
                createEngineSession(context.store, action)
            } else {
                // Initialization is in progress by a pending CreateEngineSessionAction. Let's
                // schedule dispatching the follow-up action when the engine session is ready.
                // We launch this on main to guarantee this happens after the engine session
                // is created which has been launched on main already at this point.
                action.followupAction?.let {
                    scope.launch {
                        context.store.dispatch(it)
                    }
                }
            }
        } else {
            next(action)
        }
    }

    private fun createEngineSession(
        store: Store<BrowserState, BrowserAction>,
        action: EngineAction.CreateEngineSessionAction,
    ) {
        logger.debug("Request to create engine session for tab ${action.tabId}")

        scope.launch {
            // We only need to ask for an EngineSession here. If needed this method will internally
            // create one and dispatch a LinkEngineSessionAction to add it to BrowserState.
            getOrCreateEngineSession(
                engine,
                logger,
                store,
                action.tabId,
            )

            action.followupAction?.let {
                store.dispatch(it)
            }
        }
    }
}

@MainThread
@Suppress("ReturnCount")
private fun getOrCreateEngineSession(
    engine: Engine,
    logger: Logger,
    store: Store<BrowserState, BrowserAction>,
    tabId: String,
): EngineSession? {
    val tab = store.state.findTabOrCustomTab(tabId)
    if (tab == null) {
        logger.warn("Requested engine session for tab. But tab does not exist. ($tabId)")
        return null
    }

    if (tab.engineState.crashed) {
        logger.warn("Not creating engine session, since tab is crashed. Waiting for restore.")
        return null
    }

    tab.engineState.engineSession?.let {
        logger.debug("Engine session already exists for tab $tabId")
        return it
    }

    return createEngineSession(engine, logger, store, tab)
}

@MainThread
private fun createEngineSession(
    engine: Engine,
    logger: Logger,
    store: Store<BrowserState, BrowserAction>,
    tab: SessionState,
): EngineSession {
    val engineSession = engine.createSession(tab.content.private, tab.contextId)
    logger.debug("Created engine session for tab ${tab.id}")

    val engineSessionState = tab.engineState.engineSessionState
    val skipLoading = if (engineSessionState != null) {
        engineSession.restoreState(engineSessionState)
    } else {
        false
    }

    store.dispatch(
        EngineAction.LinkEngineSessionAction(
            tab.id,
            engineSession,
            skipLoading = skipLoading,
        ),
    )

    return engineSession
}
