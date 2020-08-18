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
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.sync.SyncedTabsViewHolder.DeviceViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.ErrorViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.NoTabsViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.TabViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.TitleViewHolder
import org.mozilla.fenix.sync.ext.toAdapterList
import mozilla.components.browser.storage.sync.Tab as SyncTab
import mozilla.components.concept.sync.Device as SyncDevice

class SyncedTabsAdapter(
    private val newListener: SyncedTabsView.Listener
) : ListAdapter<SyncedTabsAdapter.AdapterItem, SyncedTabsViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SyncedTabsViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            DeviceViewHolder.LAYOUT_ID -> DeviceViewHolder(itemView)
            TabViewHolder.LAYOUT_ID -> TabViewHolder(itemView)
            ErrorViewHolder.LAYOUT_ID -> ErrorViewHolder(itemView)
            TitleViewHolder.LAYOUT_ID -> TitleViewHolder(itemView)
            NoTabsViewHolder.LAYOUT_ID -> NoTabsViewHolder(itemView)
            else -> throw IllegalStateException()
        }
    }

    override fun onBindViewHolder(holder: SyncedTabsViewHolder, position: Int) {
        holder.bind(getItem(position), newListener)
    }

    override fun getItemViewType(position: Int) = when (getItem(position)) {
            is AdapterItem.Device -> DeviceViewHolder.LAYOUT_ID
            is AdapterItem.Tab -> TabViewHolder.LAYOUT_ID
            is AdapterItem.Error -> ErrorViewHolder.LAYOUT_ID
            is AdapterItem.Title -> TitleViewHolder.LAYOUT_ID
            is AdapterItem.NoTabs -> NoTabsViewHolder.LAYOUT_ID
    }

    fun updateData(syncedTabs: List<SyncedDeviceTabs>) {
        val allDeviceTabs = syncedTabs.toAdapterList()
        submitList(allDeviceTabs)
    }

    private object DiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
        override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            when (oldItem) {
                is AdapterItem.Device ->
                    newItem is AdapterItem.Device && oldItem.device.id == newItem.device.id
                is AdapterItem.NoTabs ->
                    newItem is AdapterItem.NoTabs && oldItem.device.id == newItem.device.id
                is AdapterItem.Tab,
                is AdapterItem.Error,
                is AdapterItem.Title ->
                    oldItem == newItem
            }

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            oldItem == newItem
    }

    /**
     * The various types of adapter items that can be found in a [SyncedTabsAdapter].
     */
    sealed class AdapterItem {

        /**
         * A title header of the Synced Tabs UI that has a refresh button in it. This may be seen
         * only in some views depending on where the Synced Tabs UI is displayed.
         */
        object Title : AdapterItem()

        /**
         * A device header for displaying a synced device.
         */
        data class Device(val device: SyncDevice) : AdapterItem()

        /**
         * A tab that was synced.
         */
        data class Tab(val tab: SyncTab) : AdapterItem()

        /**
         * A placeholder for a device that has no tabs synced.
         */
        data class NoTabs(val device: SyncDevice) : AdapterItem()

        /**
         * A message displayed if an error was encountered.
         */
        data class Error(
            val descriptionResId: Int,
            val navController: NavController? = null
        ) : AdapterItem()
    }
}
