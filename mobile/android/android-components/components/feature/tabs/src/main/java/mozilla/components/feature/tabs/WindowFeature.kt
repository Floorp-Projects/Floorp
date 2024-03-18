/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.filterChanged

/**
 * Feature implementation for handling window requests by opening and closing tabs.
 */
class WindowFeature(
    private val store: BrowserStore,
    private val tabsUseCases: TabsUseCases,
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    /**
     * Starts observing the selected session to listen for window requests
     * and opens / closes tabs as needed.
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.tabs }
                .filterChanged {
                    it.content.windowRequest
                }
                .collect { state ->
                    val windowRequest = state.content.windowRequest
                    when (windowRequest?.type) {
                        WindowRequest.Type.CLOSE -> consumeWindowRequest(state.id) {
                            store.state.tabs.find { it.engineState.engineSession === windowRequest.prepare() }?.let {
                                tabsUseCases.removeTab(it.id)
                            }
                        }
                        WindowRequest.Type.OPEN -> consumeWindowRequest(state.id) {
                            tabsUseCases.addTab(
                                selectTab = true,
                                parentId = state.id,
                                engineSession = windowRequest.prepare(),
                                private = state.content.private,
                            )
                            windowRequest.start()
                        }
                        else -> {
                            // no-op
                        }
                    }
                }
        }
    }

    private fun consumeWindowRequest(
        tabId: String,
        consume: () -> Unit,
    ) {
        consume()
        store.dispatch(ContentAction.ConsumeWindowRequestAction(tabId))
    }

    /**
     * Stops observing the selected session for incoming window requests.
     */
    override fun stop() {
        scope?.cancel()
    }
}
