/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.navigation.NavController
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import org.mozilla.fenix.sync.SyncedTabsViewHolder.DeviceViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.ErrorViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.SignInViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.TabViewHolder
import mozilla.components.browser.storage.sync.Tab as SyncTab
import mozilla.components.concept.sync.Device as SyncDevice

class SyncedTabsAdapter(
    private val listener: (SyncTab) -> Unit
) : ListAdapter<SyncedTabsAdapter.AdapterItem, SyncedTabsViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SyncedTabsViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            DeviceViewHolder.LAYOUT_ID -> DeviceViewHolder(itemView)
            TabViewHolder.LAYOUT_ID -> TabViewHolder(itemView)
            ErrorViewHolder.LAYOUT_ID -> ErrorViewHolder(itemView)
            SignInViewHolder.LAYOUT_ID -> SignInViewHolder(itemView)
            else -> throw IllegalStateException()
        }
    }

    override fun onBindViewHolder(holder: SyncedTabsViewHolder, position: Int) {
        holder.bind(getItem(position), listener)
    }

    override fun getItemViewType(position: Int) = when (getItem(position)) {
        is AdapterItem.Device -> DeviceViewHolder.LAYOUT_ID
        is AdapterItem.Tab -> TabViewHolder.LAYOUT_ID
        is AdapterItem.Error -> ErrorViewHolder.LAYOUT_ID
        is AdapterItem.SignIn -> SignInViewHolder.LAYOUT_ID
    }

    fun updateData(syncedTabs: List<SyncedDeviceTabs>) {
        val allDeviceTabs = mutableListOf<AdapterItem>()

        syncedTabs.forEach { (device, tabs) ->
            if (tabs.isNotEmpty()) {
                allDeviceTabs.add(AdapterItem.Device(device))
                tabs.mapTo(allDeviceTabs) { AdapterItem.Tab(it) }
            }
        }

        submitList(allDeviceTabs)
    }

    private object DiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
        override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            when (oldItem) {
                is AdapterItem.Device ->
                    newItem is AdapterItem.Device && oldItem.device.id == newItem.device.id
                is AdapterItem.Tab, AdapterItem.Error, AdapterItem.SignIn ->
                    oldItem == newItem
            }

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            oldItem == newItem
    }

    sealed class AdapterItem {
        data class Device(val device: SyncDevice) : AdapterItem()
        data class Tab(val tab: SyncTab) : AdapterItem()
        data class SignIn(val navController: NavController) : AdapterItem()
        data class Error(val errorResId: Int) : AdapterItem()
    }
}
