/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.engine

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.helper.Target
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView

/**
 * Composes an [EngineView] obtained from the given [Engine] and renders the web content of the
 * [target] from the [store] on it.
 */
@Composable
fun WebContent(
    engine: Engine,
    store: BrowserStore,
    target: Target,
) {
    val selectedTab = target.observeAsComposableStateFrom(
        store = store,
        observe = { tab ->
            // Render if the tab itself changed or when the state of the linked engine session changes
            arrayOf(
                tab?.id,
                tab?.engineState?.engineSession,
                tab?.engineState?.crashed,
                tab?.content?.firstContentfulPaint,
            )
        },
    )

    AndroidView(
        modifier = Modifier.fillMaxSize(),
        factory = { context -> engine.createView(context).asView() },
        update = { view ->
            val engineView = view as EngineView

            val tab = selectedTab.value
            if (tab == null) {
                engineView.release()
            } else {
                val session = tab.engineState.engineSession
                if (session == null) {
                    // This tab does not have an EngineSession that we can render yet. Let's dispatch an
                    // action to request creating one. Once one was created and linked to this session, this
                    // method will get invoked again.
                    store.dispatch(EngineAction.CreateEngineSessionAction(tab.id))
                } else {
                    engineView.render(session)
                }
            }
        },
    )
}
