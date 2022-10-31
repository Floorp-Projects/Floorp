/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.customtabs.store.CustomTabState
import mozilla.components.feature.customtabs.store.CustomTabsServiceState
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.pwa.ext.getTrustedScope
import mozilla.components.feature.pwa.ext.trustedOrigins
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.net.isInScope
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Hides a custom tab toolbar for Progressive Web Apps and Trusted Web Activities.
 *
 * When the tab with [tabId] is inside a trusted scope, the toolbar will be hidden.
 * Once the tab with [tabId] navigates to another scope, the toolbar will be revealed.
 * The toolbar is also hidden in fullscreen mode or picture in picture mode.
 *
 * In standard custom tabs, no scopes are trusted.
 * As a result the URL has no impact on toolbar visibility.
 *
 * @param store Reference to the browser store where tab state is located.
 * @param customTabsStore Reference to the store that communicates with the custom tabs service.
 * @param tabId ID of the tab session, or null if the selected session should be used.
 * @param manifest Reference to the cached [WebAppManifest] for the current PWA.
 * Null if this feature is not used in a PWA context.
 * @param setToolbarVisibility Callback to show or hide the toolbar.
 */
class WebAppHideToolbarFeature(
    private val store: BrowserStore,
    private val customTabsStore: CustomTabsServiceStore,
    private val tabId: String? = null,
    manifest: WebAppManifest? = null,
    private val setToolbarVisibility: (Boolean) -> Unit,
) : LifecycleAwareFeature {

    private val manifestScope = listOfNotNull(manifest?.getTrustedScope())
    private var scope: CoroutineScope? = null

    init {
        // Hide the toolbar by default to prevent a flash.
        val tab = store.state.findTabOrCustomTabOrSelectedTab(tabId)
        val customTabState = customTabsStore.state.getCustomTabStateForTab(tab)
        setToolbarVisibility(shouldToolbarBeVisible(tab, customTabState))
    }

    override fun start() {
        scope = MainScope().apply {
            launch {
                // Since we subscribe to both store and customTabsStore,
                // we don't extend another non-external-apps feature for hiding the toolbar
                // as very little code would be shared.
                val sessionFlow = store.flow()
                    .map { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                    .ifChanged()
                val customTabServiceMapFlow = customTabsStore.flow()

                sessionFlow.combine(customTabServiceMapFlow) { tab, customTabServiceState ->
                    tab to customTabServiceState.getCustomTabStateForTab(tab)
                }
                    .map { (tab, customTabState) -> shouldToolbarBeVisible(tab, customTabState) }
                    .ifChanged()
                    .collect { toolbarVisible ->
                        setToolbarVisibility(toolbarVisible)
                    }
            }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    /**
     * Reports if the toolbar should be shown for the given external app session.
     * If the URL is in the same scope as the [WebAppManifest]
     */
    private fun shouldToolbarBeVisible(
        session: SessionState?,
        customTabState: CustomTabState?,
    ): Boolean {
        val url = session?.content?.url?.toUri() ?: return true

        val trustedOrigins = customTabState?.trustedOrigins.orEmpty()
        val inScope = url.isInScope(manifestScope + trustedOrigins)

        return !inScope && !session.content.fullScreen && !session.content.pictureInPictureEnabled
    }

    /**
     * Find corresponding custom tab state, if any.
     */
    private fun CustomTabsServiceState.getCustomTabStateForTab(
        tab: SessionState?,
    ): CustomTabState? {
        return (tab as? CustomTabSessionState)?.config?.sessionToken?.let { sessionToken ->
            tabs[sessionToken]
        }
    }
}
