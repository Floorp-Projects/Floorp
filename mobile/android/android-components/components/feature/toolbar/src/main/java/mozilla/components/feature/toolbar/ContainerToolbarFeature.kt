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
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Container toolbar implementation that updates the toolbar with the container page action
 * whenever the selected tab changes.
 */
class ContainerToolbarFeature(
    private val toolbar: Toolbar,
    private var store: BrowserStore,
) : LifecycleAwareFeature {
    private var containerPageAction: ContainerToolbarAction? = null
    private var scope: CoroutineScope? = null

    init {
        renderContainerAction(store.state)
    }

    override fun start() {
        scope = store.flowScoped { flow ->
            flow.ifChanged { it.selectedTab }
                .collect { state ->
                    renderContainerAction(state, state.selectedTab)
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun renderContainerAction(state: BrowserState, tab: SessionState? = null) {
        val containerState = state.containers[tab?.contextId]

        if (containerState == null) {
            // Entered a normal tab from a container tab. Remove the old container
            // page action.
            containerPageAction?.let {
                toolbar.removePageAction(it)
                toolbar.invalidateActions()
                containerPageAction = null
            }
            return
        } else if (containerState == containerPageAction?.container) {
            // Do nothing since we're still in a tab with same container.
            return
        }

        // Remove the old container page action and create a new action with the new
        // container state.
        containerPageAction?.let {
            toolbar.removePageAction(it)
            containerPageAction = null
        }

        containerPageAction = ContainerToolbarAction(containerState).also { action ->
            toolbar.addPageAction(action)
            toolbar.invalidateActions()
        }
    }
}
