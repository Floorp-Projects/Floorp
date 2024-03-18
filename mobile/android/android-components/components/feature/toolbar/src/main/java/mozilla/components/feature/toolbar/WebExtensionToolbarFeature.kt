/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.os.Handler
import android.os.HandlerThread
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.android.asCoroutineDispatcher
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

/**
 * Web extension toolbar implementation that updates the toolbar whenever the state of web
 * extensions changes.
 */
class WebExtensionToolbarFeature(
    private val toolbar: Toolbar,
    private var store: BrowserStore,
) : LifecycleAwareFeature {
    // This maps web extension ids to [WebExtensionToolbarAction]s for efficient
    // updates of global and tab-specific browser/page actions within the same
    // lifecycle.
    @VisibleForTesting
    internal val webExtensionBrowserActions = HashMap<String, WebExtensionToolbarAction>()
    internal val webExtensionPageActions = HashMap<String, WebExtensionToolbarAction>()

    private var scope: CoroutineScope? = null

    internal val iconThread = HandlerThread("IconThread")
    internal val iconHandler by lazy {
        iconThread.start()
        Handler(iconThread.looper)
    }

    internal var iconJobDispatcher: CoroutineDispatcher = Dispatchers.Main

    init {
        renderWebExtensionActions(store.state)
    }

    /**
     * Starts observing for the state of web extensions changes
     */
    override fun start() {
        // The feature could start with an existing view and toolbar so
        // we have to check if any stale actions (from uninstalled or
        // disabled extensions) are being displayed and remove them.
        webExtensionBrowserActions
            .filterKeys { !store.state.extensions.containsKey(it) || store.state.extensions[it]?.enabled == false }
            .forEach { (extensionId, action) ->
                toolbar.removeBrowserAction(action)
                toolbar.invalidateActions()
                webExtensionBrowserActions.remove(extensionId)
            }

        webExtensionPageActions
            .filterKeys { !store.state.extensions.containsKey(it) || store.state.extensions[it]?.enabled == false }
            .forEach { (extensionId, action) ->
                toolbar.removePageAction(action)
                toolbar.invalidateActions()
                webExtensionPageActions.remove(extensionId)
            }

        iconJobDispatcher = iconHandler.asCoroutineDispatcher("WebExtensionIconDispatcher")
        scope = store.flowScoped { flow ->
            flow.ifAnyChanged { arrayOf(it.selectedTab, it.extensions) }
                .collect { state ->
                    renderWebExtensionActions(state, state.selectedTab)
                }
        }
    }

    override fun stop() {
        iconJobDispatcher.cancel()
        scope?.cancel()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun renderWebExtensionActions(state: BrowserState, tab: SessionState? = null) {
        val extensions = state.extensions.values.toList()
        extensions.filter { it.enabled }.sortedBy { it.name }.forEach { extension ->
            if (extensionNotAllowedInTab(extension, tab)) {
                webExtensionPageActions[extension.id]?.let {
                    toolbar.removePageAction(it)
                    toolbar.invalidateActions()
                    webExtensionPageActions.remove(extension.id)
                }
                webExtensionBrowserActions[extension.id]?.let {
                    toolbar.removeBrowserAction(it)
                    toolbar.invalidateActions()
                    webExtensionBrowserActions.remove(extension.id)
                }
                return@forEach
            }

            extension.browserAction?.let { browserAction ->
                addOrUpdateAction(
                    extension = extension,
                    globalAction = browserAction,
                    tabAction = tab?.extensionState?.get(extension.id)?.browserAction,
                )
            }

            extension.pageAction?.let { pageAction ->
                val tabPageAction = tab?.extensionState?.get(extension.id)?.pageAction

                // Unlike browser actions, page actions are not displayed by default (only if enabled):
                // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/page_action
                if (pageAction.copyWithOverride(tabPageAction).enabled == true) {
                    addOrUpdateAction(
                        extension = extension,
                        globalAction = pageAction,
                        tabAction = tabPageAction,
                        isPageAction = true,
                    )
                }
            }
        }
    }

    private fun extensionNotAllowedInTab(
        extension: WebExtensionState?,
        tab: SessionState?,
    ): Boolean = extension?.allowedInPrivateBrowsing == false && tab?.content?.private == true

    private fun addOrUpdateAction(
        extension: WebExtensionState,
        globalAction: Action,
        tabAction: Action?,
        isPageAction: Boolean = false,
    ) {
        val actionMap = if (isPageAction) webExtensionPageActions else webExtensionBrowserActions
        // Add the global page/browser action if it doesn't exist
        val toolbarAction = actionMap.getOrPut(extension.id) {
            val toolbarAction = WebExtensionToolbarAction(
                action = globalAction,
                listener = globalAction.onClick,
                iconJobDispatcher = iconJobDispatcher,
            )
            if (isPageAction) {
                toolbar.addPageAction(toolbarAction)
            } else {
                toolbar.addBrowserAction(toolbarAction)
            }
            toolbar.invalidateActions()
            toolbarAction
        }

        // Apply tab-specific override of page/browser action
        tabAction?.let {
            toolbarAction.action = globalAction.copyWithOverride(it)
            toolbar.invalidateActions()
        }
    }
}
