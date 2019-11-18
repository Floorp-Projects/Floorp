/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.BrowserAction
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

typealias WebExtensionBrowserAction = BrowserAction

/**
 * Web extension toolbar implementation that updates the toolbar whenever the state of web
 * extensions changes.
 */
class WebExtensionToolbarFeature(
    private val toolbar: Toolbar,
    private var store: BrowserStore
) : LifecycleAwareFeature {
    // This maps web extension id to [WebExtensionToolbarAction]
    @VisibleForTesting
    internal val webExtensionBrowserActions = HashMap<String, WebExtensionToolbarAction>()

    private var scope: CoroutineScope? = null

    /**
     * Starts observing for the state of web extensions changes
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.ifAnyChanged { arrayOf(it.selectedTab, it.extensions) }
                .collect { state ->
                    state.selectedTab?.let { tab ->
                        renderWebExtensionActions(state, tab)
                    }
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun renderWebExtensionActions(state: BrowserState, tab: SessionState) {
        val extensionsMap = state.extensions.toMutableMap()
        extensionsMap.putAll(tab.extensionState)

        val extensions = extensionsMap.values.toList()

        extensions.forEach { extension ->
            extension.browserAction?.let { extensionAction ->
                val existingBrowserAction = webExtensionBrowserActions[extension.id]

                if (existingBrowserAction != null) {
                    existingBrowserAction.browserAction = extensionAction
                } else {
                    val toolbarAction = WebExtensionToolbarAction(
                            browserAction = extensionAction,
                            listener = extensionAction.onClick)
                    toolbar.addBrowserAction(toolbarAction)
                    webExtensionBrowserActions[extension.id] = toolbarAction
                }
                toolbar.invalidateActions()
            }
        }
    }
}
