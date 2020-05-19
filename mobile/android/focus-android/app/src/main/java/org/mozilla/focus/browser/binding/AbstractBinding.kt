/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.binding

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Helper class for creating small binding classes that are responsible for reacting to state
 * changes.
 */
abstract class AbstractBinding(
    private val store: BrowserStore
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null

    override fun start() {
        scope = store.flowScoped { flow ->
            onState(flow)
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    abstract suspend fun onState(flow: Flow<BrowserState>)
}
