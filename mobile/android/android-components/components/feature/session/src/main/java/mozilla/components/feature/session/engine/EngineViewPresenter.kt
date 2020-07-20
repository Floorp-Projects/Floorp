/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.engine

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.session.usecases.EngineSessionUseCases
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Presenter implementation for EngineView.
 */
internal class EngineViewPresenter(
    private val store: BrowserStore,
    private val engineView: EngineView,
    private val useCases: EngineSessionUseCases,
    private val tabId: String?
) {
    private var scope: CoroutineScope? = null

    /**
     * Start presenter and display data in view.
     */
    fun start() {
        scope = store.flowScoped { flow ->
            flow.map { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .map { tab -> tab?.let { useCases.getOrCreateEngineSession(it.id) } }
                .ifChanged()
                .collect { engineSession -> onEngineSession(engineSession) }
        }
    }

    /**
     * Stop presenter from updating view.
     */
    fun stop() {
        scope?.cancel()
    }

    private fun onEngineSession(engineSession: EngineSession?) {
        if (engineSession == null) {
            // With no EngineSession being available to render anymore we could call release here
            // to make sure GeckoView also frees any references to the previous session. However
            // we are seeing problems with that and are temporarily disabling this to investigate.
            // https://github.com/mozilla-mobile/android-components/issues/7753
            //
            // engineView.release()
        } else {
            engineView.render(engineSession)
        }
    }
}
