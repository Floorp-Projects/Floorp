/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.view.View
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature implementation to add pull to refresh functionality to browsers.
 *
 * @param swipeRefreshLayout Reference to SwipeRefreshLayout that has an [EngineView] as its child.
 */
class SwipeRefreshFeature(
    sessionManager: SessionManager,
    private val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
    private val swipeRefreshLayout: SwipeRefreshLayout,
    private val sessionId: String? = null
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature,
    SwipeRefreshLayout.OnChildScrollUpCallback, SwipeRefreshLayout.OnRefreshListener {

    init {
        swipeRefreshLayout.setOnRefreshListener(this)
        swipeRefreshLayout.setOnChildScrollUpCallback(this)
    }

    /**
     * Start feature: Starts adding pull to refresh behavior for the active session.
     */
    override fun start() {
        observeIdOrSelected(sessionId)
    }

    /**
     * Callback that checks whether it is possible for the child view to scroll up.
     * If the child view cannot scroll up, attempted to scroll up triggers a refresh gesture.
     */
    override fun canChildScrollUp(parent: SwipeRefreshLayout, child: View?) =
        if (child is EngineView) {
            val result = child.canScrollVerticallyUp()
            result
        } else {
            true
        }

    /**
     * Called when a swipe gesture triggers a refresh.
     */
    override fun onRefresh() {
        reloadUrlUseCase(activeSession)
    }

    /**
     * Called when the current session starts or stops refreshing.
     */
    override fun onLoadingStateChanged(session: Session, loading: Boolean) {
        // Don't activate the indicator unless triggered by a gesture.
        if (!loading) {
            swipeRefreshLayout.isRefreshing = loading
        }
    }
}
