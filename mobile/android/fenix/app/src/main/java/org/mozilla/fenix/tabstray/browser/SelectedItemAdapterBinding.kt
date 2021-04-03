/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.flow.map
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * Notifies the adapter when the selection mode changes.
 */
class SelectedItemAdapterBinding(
    val store: TabsTrayStore,
    val adapter: BrowserTabsAdapter
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.map { it.mode }
                // ignore initial mode update; the adapter is already in an updated state.
                .drop(1)
                .ifChanged()
                .collect { mode ->
                    notifyAdapter(mode)
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    private fun notifyAdapter(mode: Mode) = with(adapter) {
        if (mode == Mode.Normal) {
            notifyItemRangeChanged(0, itemCount, PAYLOAD_HIGHLIGHT_SELECTED_ITEM)
        } else {
            notifyItemRangeChanged(0, itemCount, PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM)
        }
    }
}
