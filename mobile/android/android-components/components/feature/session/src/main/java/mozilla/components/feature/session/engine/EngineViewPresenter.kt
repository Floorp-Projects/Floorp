/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.engine

import android.view.View
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

/**
 * Presenter implementation for EngineView.
 */
internal class EngineViewPresenter(
    private val store: BrowserStore,
    private val engineView: EngineView,
    private val tabId: String?,
) {
    private var scope: CoroutineScope? = null

    /**
     * Start presenter and display data in view.
     */
    fun start() {
        scope = store.flowScoped { flow ->
            flow.map { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                // Render if the tab itself changed and when an engine session is linked
                .ifAnyChanged { tab ->
                    arrayOf(
                        tab?.id,
                        tab?.engineState?.engineSession,
                        tab?.engineState?.crashed,
                        tab?.content?.firstContentfulPaint,
                    )
                }
                .collect { tab -> onTabToRender(tab) }
        }
    }

    /**
     * Stop presenter from updating view.
     */
    fun stop() {
        scope?.cancel()
        engineView.release()
    }

    private fun onTabToRender(tab: SessionState?) {
        if (tab == null) {
            engineView.release()
        } else {
            renderTab(tab)
        }
    }

    private fun renderTab(tab: SessionState) {
        val engineSession = tab.engineState.engineSession

        val actualView = engineView.asView()

        if (tab.engineState.crashed) {
            engineView.release()
            return
        }

        if (tab.content.firstContentfulPaint) {
            actualView.visibility = View.VISIBLE
        }

        if (engineSession == null) {
            // This tab does not have an EngineSession that we can render yet. Let's dispatch an
            // action to request creating one. Once one was created and linked to this session, this
            // method will get invoked again.
            store.dispatch(EngineAction.CreateEngineSessionAction(tab.id))
        } else {
            // Since we render the tab again let's update its last access flag. In the future, we
            // may need more fine-grained flags to differentiate viewing from tab selection.
            store.dispatch(LastAccessAction.UpdateLastAccessAction(tab.id, System.currentTimeMillis()))
            engineView.render(engineSession)
        }
    }
}
