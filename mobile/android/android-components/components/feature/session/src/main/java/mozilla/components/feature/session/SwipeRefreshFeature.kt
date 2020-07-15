/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature implementation to add pull to refresh functionality to browsers.
 *
 * @param swipeRefreshLayout Reference to SwipeRefreshLayout that has an [EngineView] as its child.
 */
class SwipeRefreshFeature(
    private val store: BrowserStore,
    private val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
    private val swipeRefreshLayout: SwipeRefreshLayout,
    private val tabId: String? = null
) : LifecycleAwareFeature, SwipeRefreshLayout.OnChildScrollUpCallback, SwipeRefreshLayout.OnRefreshListener {
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
                .map { tab -> tab?.content?.loading ?: false }
                .ifChanged()
                .collect { loading ->
                    onLoadingStateChanged(loading)
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
    override fun canChildScrollUp(parent: SwipeRefreshLayout, child: View?) =
        if (child is EngineView) {
            child.canScrollVerticallyUp() ||
                (child.getInputResult() == EngineView.InputResult.INPUT_RESULT_HANDLED_CONTENT)
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

    /**
     * Called when the current session starts or stops refreshing.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun onLoadingStateChanged(loading: Boolean) {
        // Don't activate the indicator unless triggered by a gesture.
        if (!loading) {
            swipeRefreshLayout.isRefreshing = loading
        }
    }
}
