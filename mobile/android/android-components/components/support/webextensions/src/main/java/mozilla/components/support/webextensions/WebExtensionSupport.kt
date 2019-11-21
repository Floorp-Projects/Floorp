/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.ActionHandler
import mozilla.components.concept.engine.webextension.BrowserAction
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionDelegate
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.filterChanged

/**
 * Provides functionality to make sure web extension related events in the
 * [Engine] are reflected in the browser state by dispatching the
 * corresponding actions to the [BrowserStore].
 */
object WebExtensionSupport {
    @VisibleForTesting
    internal val installedExtensions = mutableSetOf<WebExtension>()

    /**
     * [ActionHandler] for session-specific overrides. Forwards actions to the
     * the provided [store].
     */
    private class SessionActionHandler(
        private val store: BrowserStore,
        private val sessionId: String
    ) : ActionHandler {

        override fun onBrowserAction(webExtension: WebExtension, session: EngineSession?, action: BrowserAction) {
            store.dispatch(WebExtensionAction.UpdateTabBrowserAction(sessionId, webExtension.id, action))
        }
    }

    /**
     * Registers a listener for web extension related events on the provided
     * [Engine] and reacts by dispatching the corresponding actions to the
     * provided [BrowserStore].
     *
     * @param engine the browser [Engine] to use.
     * @param store the application's [BrowserStore].
     * @param onNewTabOverride (optional) override of behaviour that should
     * be triggered when web extensions open a new tab e.g. when dispatching
     * to the store isn't sufficient while migrating from browser-session
     * to browser-state. This is a lambda accepting the [WebExtension], the
     * [EngineSession] to use, as well as the URL to load.
     * @param onCloseTabOverride (optional) override of behaviour that should
     * be triggered when web extensions close tabs e.g. when dispatching
     * to the store isn't sufficient while migrating from browser-session
     * to browser-state. This is a lambda  accepting the [WebExtension] and
     * the session/tab ID to close.
     */
    fun initialize(
        engine: Engine,
        store: BrowserStore,
        onNewTabOverride: ((WebExtension?, EngineSession, String) -> Unit)? = null,
        onCloseTabOverride: ((WebExtension?, String) -> Unit)? = null
    ) {
        // Observe the store and register action handlers for newly added engine sessions
        registerActionHandlersForNewSessions(store)

        engine.registerWebExtensionDelegate(object : WebExtensionDelegate {
            override fun onNewTab(webExtension: WebExtension?, url: String, engineSession: EngineSession) {
                onNewTabOverride?.invoke(webExtension, engineSession, url)
                    ?: store.dispatch(TabListAction.AddTabAction(createTab(url)))
            }

            override fun onCloseTab(webExtension: WebExtension?, engineSession: EngineSession): Boolean {
                val tab = store.state.tabs.find { it.engineState.engineSession === engineSession }
                return if (tab != null) {
                    onCloseTabOverride?.invoke(webExtension, tab.id)
                        ?: store.dispatch(TabListAction.RemoveTabAction(tab.id))
                    true
                } else {
                    false
                }
            }

            override fun onInstalled(webExtension: WebExtension) {
                installedExtensions.add(webExtension)
                store.dispatch(WebExtensionAction.InstallWebExtension(webExtension.toState()))

                // Register a global default action handler on the new extension
                webExtension.registerActionHandler(object : ActionHandler {
                    override fun onBrowserAction(
                        webExtension: WebExtension,
                        session: EngineSession?,
                        action: BrowserAction
                    ) {
                        store.dispatch(WebExtensionAction.UpdateBrowserAction(webExtension.id, action))
                    }
                })

                // Register action handler for all existing engine sessions on the new extension
                store.state.tabs
                    .forEach {
                        val session = it.engineState.engineSession
                        if (session != null) {
                            registerSessionActionHandler(webExtension, session, SessionActionHandler(store, it.id))
                        }
                    }
            }
        })
    }

    /**
     * Observes the provided store to register session-specific [ActionHandler]s
     * for all installed extensions on newly added sessions.
     */
    private fun registerActionHandlersForNewSessions(store: BrowserStore) {
        // We need to observe for the entire lifetime of the application,
        // as web extension support is not tied to any particular view.
        store.flowScoped { flow ->
            flow.mapNotNull { state -> state.tabs }
                .filterChanged {
                    it.engineState.engineSession
                }
                .collect { state ->
                    state.engineState.engineSession?.let { session ->
                        installedExtensions.forEach {
                            registerSessionActionHandler(it, session, SessionActionHandler(store, state.id))
                        }
                    }
                }
        }
    }

    private fun registerSessionActionHandler(ext: WebExtension, session: EngineSession, handler: SessionActionHandler) {
        if (ext.supportActions && !ext.hasActionHandler(session)) {
            ext.registerActionHandler(session, handler)
        }
    }

    private fun WebExtension.toState() = WebExtensionState(id, url)
}
