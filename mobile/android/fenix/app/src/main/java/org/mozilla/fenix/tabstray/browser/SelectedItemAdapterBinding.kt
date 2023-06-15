/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * Notifies the adapter when the selection mode changes.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class SelectedItemAdapterBinding(
    store: TabsTrayStore,
    val adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
) : AbstractBinding<TabsTrayState>(store) {

    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it.mode }
            .distinctUntilChanged()
            .collect { mode ->
                notifyAdapter(mode)
            }
    }

    /**
     * N.B: This method should be made more performant to find the position of the multi-selected tab that has
     * changed in the adapter, and then [RecyclerView.Adapter.notifyItemChanged].
     */
    private fun notifyAdapter(mode: Mode) = with(adapter) {
        if (mode == Mode.Normal) {
            notifyItemRangeChanged(0, itemCount, PAYLOAD_HIGHLIGHT_SELECTED_ITEM)
        } else {
            notifyItemRangeChanged(0, itemCount, PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM)
        }
    }
}
