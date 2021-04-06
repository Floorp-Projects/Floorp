/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.ui.tabcounter.TabCounter

/**
 * Updates the tab counter to the size of [BrowserState.normalTabs].
 */
class TabCounterBinding(
    private val store: BrowserStore,
    private val counter: TabCounter
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.map { it.normalTabs }
                .ifChanged()
                .collect {
                    counter.setCount(it.size)
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
