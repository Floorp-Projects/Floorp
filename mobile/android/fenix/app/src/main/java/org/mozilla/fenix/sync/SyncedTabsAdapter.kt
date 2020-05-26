/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.concept.sync.Device as SyncDevice
import mozilla.components.browser.storage.sync.Tab as SyncTab
import org.mozilla.fenix.sync.SyncedTabsViewHolder.DeviceViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.TabViewHolder

class SyncedTabsAdapter(
    private val listener: (SyncTab) -> Unit
) : ListAdapter<SyncedTabsAdapter.AdapterItem, SyncedTabsViewHolder>(
    DiffCallback
) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SyncedTabsViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            DeviceViewHolder.LAYOUT_ID -> DeviceViewHolder(itemView)
            TabViewHolder.LAYOUT_ID -> TabViewHolder(itemView)
            else -> throw IllegalStateException()
        }
    }

    override fun onBindViewHolder(holder: SyncedTabsViewHolder, position: Int) {
        val item = when (holder) {
            is DeviceViewHolder -> getItem(position) as AdapterItem.Device
            is TabViewHolder -> getItem(position) as AdapterItem.Tab
        }
        holder.bind(item, listener)
    }

    override fun getItemViewType(position: Int): Int {
        return when (getItem(position)) {
            is AdapterItem.Device -> DeviceViewHolder.LAYOUT_ID
            is AdapterItem.Tab -> TabViewHolder.LAYOUT_ID
        }
    }

    private object DiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
        override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            areContentsTheSame(oldItem, newItem)

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            oldItem == newItem
    }

    sealed class AdapterItem {
        data class Device(val device: SyncDevice) : AdapterItem()
        data class Tab(val tab: SyncTab) : AdapterItem()
    }
}
