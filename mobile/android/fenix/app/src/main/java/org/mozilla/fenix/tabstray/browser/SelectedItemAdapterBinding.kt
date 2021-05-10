/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.flow.map
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * Notifies the adapter when the selection mode changes.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class SelectedItemAdapterBinding(
    store: TabsTrayStore,
    val adapter: BrowserTabsAdapter
) : AbstractBinding<TabsTrayState>(store) {

    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it.mode }
            // ignore initial mode update; the adapter is already in an updated state.
            .drop(1)
            .ifChanged()
            .collect { mode ->
                notifyAdapter(mode)
            }
    }

    private fun notifyAdapter(mode: Mode) = with(adapter) {
        if (mode == Mode.Normal) {
            notifyItemRangeChanged(0, itemCount, PAYLOAD_HIGHLIGHT_SELECTED_ITEM)
        } else {
            notifyItemRangeChanged(0, itemCount, PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM)
        }
    }
}
