/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.view.View
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.action.ContentAction.UpdateRefreshCanceledStateAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

/**
 * Feature implementation to add pull to refresh functionality to browsers.
 *
 * @param swipeRefreshLayout Reference to SwipeRefreshLayout that has an [EngineView] as its child.
 */
class SwipeRefreshFeature(
    private val store: BrowserStore,
    private val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
    private val swipeRefreshLayout: SwipeRefreshLayout,
    private val tabId: String? = null,
) : LifecycleAwareFeature,
    SwipeRefreshLayout.OnChildScrollUpCallback,
    SwipeRefreshLayout.OnRefreshListener {
    private var scope: CoroutineScope? = null

    init {
        swipeRefreshLayout.setOnRefreshListener(this)
        swipeRefreshLayout.setOnChildScrollUpCallback(this)
    }

    /**
     * Start feature: Starts adding pull to refresh behavior for the active session.
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.map { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .ifAnyChanged {
                    arrayOf(it?.content?.loading, it?.content?.refreshCanceled)
                }
                .collect { tab ->
                    tab?.let {
                        if (!tab.content.loading || tab.content.refreshCanceled) {
                            swipeRefreshLayout.isRefreshing = false
                            if (tab.content.refreshCanceled) {
                                // In case the user tries to refresh again
                                // we need to reset refreshCanceled, to be able to
                                // get a subsequent event.
                                store.dispatch(UpdateRefreshCanceledStateAction(tab.id, false))
                            }
                        }
                    }
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    /**
     * Callback that checks whether it is possible for the child view to scroll up.
     * If the child view cannot scroll up and the scroll event is not handled by the webpage
     * it means we need to trigger the pull down to refresh functionality.
     */
    @Suppress("Deprecation")
    override fun canChildScrollUp(parent: SwipeRefreshLayout, child: View?) =
        if (child is EngineView) {
            !child.getInputResultDetail().canOverscrollTop()
        } else {
            true
        }

    /**
     * Called when a swipe gesture triggers a refresh.
     */
    override fun onRefresh() {
        store.state.findTabOrCustomTabOrSelectedTab(tabId)?.let { tab ->
            reloadUrlUseCase(tab.id)
        }
    }
}
