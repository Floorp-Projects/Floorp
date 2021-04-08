/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * Notifies whether a tab is accessible for using the swipe-to-delete gesture.
 */
class SwipeToDeleteBinding(
    private val store: TabsTrayStore
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null
    var isSwipeable = false
        private set

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.map { it.mode }
                .ifChanged()
                .collect { mode ->
                    isSwipeable = mode == TabsTrayState.Mode.Normal
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
