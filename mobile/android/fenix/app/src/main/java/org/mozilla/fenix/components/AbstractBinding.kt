/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.annotation.CallSuper
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Helper class for creating small binding classes that are responsible for reacting to state
 * changes.
 *
 * Taken with ♥️ from Focus.
 */
abstract class AbstractBinding<in S : State>(
    private val store: Store<S, out Action>
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null

    @OptIn(ExperimentalCoroutinesApi::class)
    @CallSuper
    override fun start() {
        scope = store.flowScoped { flow ->
            onState(flow)
        }
    }

    @CallSuper
    override fun stop() {
        scope?.cancel()
    }

    abstract suspend fun onState(flow: Flow<S>)
}
