/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionDelegate

/**
 * Provides functionality to make sure web extension related events in the
 * [Engine] are reflected in the browser state by dispatching the
 * corresponding actions to the [BrowserStore].
 */
object WebExtensionSupport {

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
                store.dispatch(WebExtensionAction.InstallWebExtension(webExtension.toState()))
            }
        })
    }

    private fun WebExtension.toState() = WebExtensionState(id, url)
}
