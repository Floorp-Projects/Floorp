/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection
import mozilla.components.feature.toolbar.internal.URLRenderer
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

typealias WebExtensionBrowserAction = mozilla.components.concept.engine.webextension.BrowserAction

/**
 * Presenter implementation for a toolbar implementation in order to update the toolbar whenever
 * the state of the selected session or web extensions changes.
 */
@Suppress("TooManyFunctions")
class ToolbarPresenter(
    private val toolbar: Toolbar,
    private val store: BrowserStore,
    private val customTabId: String? = null,
    urlRenderConfiguration: ToolbarFeature.UrlRenderConfiguration? = null
) {
    @VisibleForTesting
    internal var renderer = URLRenderer(toolbar, urlRenderConfiguration)

    private var scope: CoroutineScope? = null

    // This maps web extension id to [WebExtensionToolbarAction]
    @VisibleForTesting
    internal val webExtensionBrowserActions = HashMap<String, WebExtensionToolbarAction>()

    /**
     * Start presenter: Display data in toolbar.
     */
    fun start() {
        renderer.start()

        scope = store.flowScoped { flow ->
            flow.ifAnyChanged { arrayOf(it.findCustomTabOrSelectedTab(customTabId), it.extensions) }
                .collect { state ->
                    render(state)
                }
        }
    }

    fun stop() {
        scope?.cancel()
        renderer.stop()
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun render(state: BrowserState) {
        val tab = state.findCustomTabOrSelectedTab(customTabId)

        if (tab != null) {
            renderer.post(tab.content.url)

            toolbar.setSearchTerms(tab.content.searchTerms)
            toolbar.displayProgress(tab.content.progress)

            toolbar.siteSecure = if (tab.content.securityInfo.secure) {
                Toolbar.SiteSecurity.SECURE
            } else {
                Toolbar.SiteSecurity.INSECURE
            }

            toolbar.siteTrackingProtection = when {
                tab.trackingProtection.ignoredOnTrackingProtection -> SiteTrackingProtection.OFF_FOR_A_SITE
                tab.trackingProtection.enabled && tab.trackingProtection.blockedTrackers.isNotEmpty() ->
                    SiteTrackingProtection.ON_TRACKERS_BLOCKED

                tab.trackingProtection.enabled -> SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED

                else -> SiteTrackingProtection.OFF_GLOBALLY
            }

            renderWebExtensionActions(state, tab)
        } else {
            clear()
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
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

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun clear() {
        renderer.post("")

        toolbar.setSearchTerms("")
        toolbar.displayProgress(0)

        toolbar.siteSecure = Toolbar.SiteSecurity.INSECURE

        toolbar.siteTrackingProtection = SiteTrackingProtection.OFF_GLOBALLY
    }
}
