/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.EngineAction
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
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.filterChanged
import java.util.concurrent.ConcurrentHashMap

/**
 * Provides functionality to make sure web extension related events in the
 * [Engine] are reflected in the browser state by dispatching the
 * corresponding actions to the [BrowserStore].
 *
 * Note that this class can be removed once the browser-state migration
 * is completed and the [Engine] has direct access to the [BrowserStore]:
 * https://github.com/orgs/mozilla-mobile/projects/31
 */
object WebExtensionSupport {
    private val logger = Logger("mozac-webextensions")
    private var onUpdatePermissionRequest: ((
        current: WebExtension,
        updated: WebExtension,
        newPermissions: List<String>,
        onPermissionsGranted: (Boolean) -> Unit
    ) -> Unit)? = null

    val installedExtensions = ConcurrentHashMap<String, WebExtension>()

    /**
     * A [Deferred] completed during [initialize] once the state of all
     * installed extensions is known.
     */
    private val initializationResult = CompletableDeferred<Unit>()

    /**
     * [ActionHandler] for session-specific overrides. Forwards actions to the
     * the provided [store].
     */
    private class SessionActionHandler(
        private val store: BrowserStore,
        private val sessionId: String
    ) : ActionHandler {

        override fun onBrowserAction(extension: WebExtension, session: EngineSession?, action: BrowserAction) {
            store.dispatch(WebExtensionAction.UpdateTabBrowserAction(sessionId, extension.id, action))
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
     * [EngineSession] to use, as well as the URL to load, return the ID of
     * the created session.
     * @param onCloseTabOverride (optional) override of behaviour that should
     * be triggered when web extensions close tabs e.g. when dispatching
     * to the store isn't sufficient while migrating from browser-session
     * to browser-state. This is a lambda accepting the [WebExtension] and
     * the session/tab ID to close.
     * @param onSelectTabOverride (optional) override of behaviour that should
     * be triggered when a tab is selected to display a web extension popup.
     * This is a lambda accepting the [WebExtension] and the session/tab ID to
     * select.
     * @param onUpdatePermissionRequest (optional) Invoked when a web extension has changed its
     * permissions while trying to update to a new version. This requires user interaction as
     * the updated extension will not be installed, until the user grants the new permissions.
     */
    @Suppress("MaxLineLength", "LongParameterList")
    fun initialize(
        engine: Engine,
        store: BrowserStore,
        onNewTabOverride: ((WebExtension?, EngineSession, String) -> String)? = null,
        onCloseTabOverride: ((WebExtension?, String) -> Unit)? = null,
        onSelectTabOverride: ((WebExtension?, String) -> Unit)? = null,
        onUpdatePermissionRequest: ((current: WebExtension, updated: WebExtension, newPermissions: List<String>, onPermissionsGranted: ((Boolean) -> Unit)) -> Unit)? = { _, _, _, _ -> }
    ) {
        this.onUpdatePermissionRequest = onUpdatePermissionRequest

        // Queries the engine for installed extensions and adds them to the store
        registerInstalledExtensions(store, engine)

        // Observe the store and register action handlers for newly added engine sessions
        registerActionHandlersForNewSessions(store)

        engine.registerWebExtensionDelegate(object : WebExtensionDelegate {
            override fun onNewTab(webExtension: WebExtension?, url: String, engineSession: EngineSession) {
                openTab(store, onNewTabOverride, webExtension, engineSession, url)
            }

            override fun onCloseTab(webExtension: WebExtension?, engineSession: EngineSession): Boolean {
                val tab = store.state.tabs.find { it.engineState.engineSession === engineSession }
                return if (tab != null) {
                    closeTab(tab.id, store, onCloseTabOverride, webExtension)
                    true
                } else {
                    false
                }
            }

            override fun onBrowserActionDefined(webExtension: WebExtension, action: BrowserAction) {
                store.dispatch(WebExtensionAction.UpdateBrowserAction(webExtension.id, action))
            }

            override fun onToggleBrowserActionPopup(
                webExtension: WebExtension,
                engineSession: EngineSession,
                action: BrowserAction
            ): EngineSession? {
                val popupSessionId = store.state.extensions[webExtension.id]?.browserActionPopupSession
                return if (popupSessionId != null && store.state.tabs.find { it.id == popupSessionId } != null) {
                    if (popupSessionId == store.state.selectedTabId) {
                        closeTab(popupSessionId, store, onCloseTabOverride, webExtension)
                    } else {
                        onSelectTabOverride?.invoke(webExtension, popupSessionId)
                                ?: store.dispatch(TabListAction.SelectTabAction(popupSessionId))
                    }
                    null
                } else {
                    val sessionId = openTab(store, onNewTabOverride, webExtension, engineSession, "")
                    store.dispatch(WebExtensionAction.UpdateBrowserActionPopupSession(webExtension.id, sessionId))
                    engineSession
                }
            }

            override fun onInstalled(webExtension: WebExtension) {
                registerInstalledExtension(store, webExtension)
            }

            override fun onUninstalled(webExtension: WebExtension) {
                installedExtensions.remove(webExtension.id)
                store.dispatch(WebExtensionAction.UninstallWebExtensionAction(webExtension.id))
            }

            override fun onEnabled(webExtension: WebExtension) {
                installedExtensions[webExtension.id] = webExtension
                store.dispatch(WebExtensionAction.UpdateWebExtensionEnabledAction(webExtension.id, true))
            }

            override fun onDisabled(webExtension: WebExtension) {
                installedExtensions[webExtension.id] = webExtension
                store.dispatch(WebExtensionAction.UpdateWebExtensionEnabledAction(webExtension.id, false))
            }

            override fun onInstallPermissionRequest(webExtension: WebExtension): Boolean {
                // Our current installation flow has us approve permissions before we call
                // install on the engine. Therefore we can just approve the permission request
                // here during installation.
                return true
            }

            override fun onUpdatePermissionRequest(
                current: WebExtension,
                updated: WebExtension,
                newPermissions: List<String>,
                onPermissionsGranted: ((Boolean) -> Unit)
            ) {
                this@WebExtensionSupport.onUpdatePermissionRequest?.invoke(
                    current,
                    updated,
                    newPermissions,
                    onPermissionsGranted
                )
            }
        })
    }

    /**
     * Awaits for completion of the initialization process (completes when the
     * state of all installed extensions is known).
     */
    suspend fun awaitInitialization() = initializationResult.await()

    /**
     * Queries the [engine] for installed web extensions and adds them to the [store].
     */
    private fun registerInstalledExtensions(store: BrowserStore, engine: Engine) {
        engine.listInstalledWebExtensions(
            onSuccess = {
                extensions -> extensions.forEach { registerInstalledExtension(store, it) }
                initializationResult.complete(Unit)
            },
            onError = {
                throwable -> logger.error("Failed to query installed extension", throwable)
                initializationResult.completeExceptionally(throwable)
            }
        )
    }

    /**
     * Marks the provided [webExtension] as installed by adding it to the [store].
     */
    private fun registerInstalledExtension(store: BrowserStore, webExtension: WebExtension) {
        installedExtensions[webExtension.id] = webExtension
        store.dispatch(WebExtensionAction.InstallWebExtensionAction(webExtension.toState()))

        // Register action handler for all existing engine sessions on the new extension,
        // an issue was filed to get us an API, so we don't have to do this per extension:
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1603559
        store.state.tabs
            .forEach { tab ->
                tab.engineState.engineSession?.let { session ->
                    registerSessionActionHandler(webExtension, session, SessionActionHandler(store, tab.id))
                }
            }
    }

    /**
     * Marks the provided [updatedExtension] as updated in the [store].
     */
    fun markExtensionAsUpdated(store: BrowserStore, updatedExtension: WebExtension) {
        installedExtensions[updatedExtension.id] = updatedExtension
        store.dispatch(WebExtensionAction.UpdateWebExtension(updatedExtension.toState()))

        // Register action handler for all existing engine sessions on the new extension
        store.state.tabs.forEach { tab ->
            tab.engineState.engineSession?.let { session ->
                registerSessionActionHandler(
                    updatedExtension,
                    session,
                    SessionActionHandler(store, tab.id)
                )
            }
        }
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
                        installedExtensions.values.forEach {
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

    private fun openTab(
        store: BrowserStore,
        onNewTabOverride: ((WebExtension?, EngineSession, String) -> String)? = null,
        webExtension: WebExtension?,
        engineSession: EngineSession,
        url: String
    ): String {
        return if (onNewTabOverride != null) {
            onNewTabOverride.invoke(webExtension, engineSession, url)
        } else {
            val tab = createTab(url)
            store.dispatch(TabListAction.AddTabAction(tab))
            store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession))
            tab.id
        }
    }

    private fun closeTab(
        id: String,
        store: BrowserStore,
        onCloseTabOverride: ((WebExtension?, String) -> Unit)? = null,
        webExtension: WebExtension?
    ) {
        onCloseTabOverride?.invoke(webExtension, id) ?: store.dispatch(TabListAction.RemoveTabAction(id))
    }

    private fun WebExtension.toState() = WebExtensionState(id, url)
}
