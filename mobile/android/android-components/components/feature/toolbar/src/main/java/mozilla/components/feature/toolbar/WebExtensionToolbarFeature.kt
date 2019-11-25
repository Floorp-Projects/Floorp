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
        extensions.forEach { extension ->
            extension.browserAction?.let { browserAction ->
                // Add the global browser action if it doesn't exist
                val toolbarAction = webExtensionBrowserActions.getOrPut(extension.id) {
                    val toolbarAction = WebExtensionToolbarAction(
                        browserAction = browserAction,
                        listener = browserAction.onClick,
                        iconJobDispatcher = iconJobDispatcher
                    )
                    toolbar.addBrowserAction(toolbarAction)
                    toolbar.invalidateActions()
                    toolbarAction
                }

                // Check if we have a tab-specific override and apply it.
                // Note that tab-specific actions only contain the values that
                // should be overridden. All other values will be null and should
                // therefore *not* be applied / overridden.
                tab?.extensionState?.get(extension.id)?.browserAction?.let {
                    toolbarAction.browserAction = BrowserAction(
                        title = it.title ?: browserAction.title,
                        enabled = it.enabled ?: browserAction.enabled,
                        badgeText = it.badgeText ?: browserAction.badgeText,
                        badgeBackgroundColor = it.badgeBackgroundColor ?: browserAction.badgeBackgroundColor,
                        badgeTextColor = it.badgeTextColor ?: browserAction.badgeTextColor,
                        loadIcon = it.loadIcon ?: browserAction.loadIcon,
                        onClick = it.onClick
                    )
                    toolbar.invalidateActions()
                }
            }
        }
    }
}
