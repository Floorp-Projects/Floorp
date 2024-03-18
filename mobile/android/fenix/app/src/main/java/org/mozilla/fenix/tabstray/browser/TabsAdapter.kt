/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.annotation.CallSuper
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.browser.tabstray.TabsTrayStyling

/**
 * RecyclerView adapter implementation to display a list/grid of tabs.
 *
 * The previous tabs adapter was very restrictive and required Fenix to jump through
 * may hoops to access and update certain methods. An abstract adapter is easier to manage
 * for Android UI APIs.
 *
 * TODO Let's upstream this to AC with tests.
 */
abstract class TabsAdapter<T : TabViewHolder>(
    val delegate: TabsTray.Delegate,
) : ListAdapter<TabSessionState, T>(DiffCallback), TabsTray {

    protected var selectedTabId: String? = null
    protected var styling: TabsTrayStyling = TabsTrayStyling()

    @CallSuper
    override fun updateTabs(tabs: List<TabSessionState>, tabPartition: TabPartition?, selectedTabId: String?) {
        this.selectedTabId = selectedTabId

        submitList(tabs)
    }

    @CallSuper
    override fun onBindViewHolder(holder: T, position: Int) {
        val tab = getItem(position)
        holder.bind(getItem(position), tab.id == selectedTabId, styling, delegate)
    }

    private object DiffCallback : DiffUtil.ItemCallback<TabSessionState>() {
        override fun areItemsTheSame(oldItem: TabSessionState, newItem: TabSessionState): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: TabSessionState, newItem: TabSessionState): Boolean {
            return oldItem == newItem
        }
    }
}
