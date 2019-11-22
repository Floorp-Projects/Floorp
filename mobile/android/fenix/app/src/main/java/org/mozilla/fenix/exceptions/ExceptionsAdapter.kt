/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.exceptions.viewholders.ExceptionsDeleteButtonViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsHeaderViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsListItemViewHolder

sealed class AdapterItem {
    object DeleteButton : AdapterItem()
    object Header : AdapterItem()
    data class Item(val item: ExceptionsItem) : AdapterItem()
}

/**
 * Adapter for a list of sites that are exempted from Tracking Protection,
 * along with controls to remove the exception.
 */
class ExceptionsAdapter(
    private val interactor: ExceptionsInteractor
) : ListAdapter<AdapterItem, RecyclerView.ViewHolder>(DiffCallback) {

    /**
     * Change the list of items that are displayed.
     * Header and footer items are added to the list as well.
     */
    fun updateData(exceptions: List<ExceptionsItem>) {
        val adapterItems = mutableListOf<AdapterItem>()
        adapterItems.add(AdapterItem.Header)
        exceptions.mapTo(adapterItems) { AdapterItem.Item(it) }
        adapterItems.add(AdapterItem.DeleteButton)
        submitList(adapterItems)
    }

    override fun getItemViewType(position: Int) = when (getItem(position)) {
        AdapterItem.DeleteButton -> ExceptionsDeleteButtonViewHolder.LAYOUT_ID
        AdapterItem.Header -> ExceptionsHeaderViewHolder.LAYOUT_ID
        is AdapterItem.Item -> ExceptionsListItemViewHolder.LAYOUT_ID
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            ExceptionsDeleteButtonViewHolder.LAYOUT_ID -> ExceptionsDeleteButtonViewHolder(
                view,
                interactor
            )
            ExceptionsHeaderViewHolder.LAYOUT_ID -> ExceptionsHeaderViewHolder(view)
            ExceptionsListItemViewHolder.LAYOUT_ID -> ExceptionsListItemViewHolder(view, interactor)
            else -> throw IllegalStateException()
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        if (holder is ExceptionsListItemViewHolder) {
            val adapterItem = getItem(position) as AdapterItem.Item
            holder.bind(adapterItem.item)
        }
    }

    private object DiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
        override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            areContentsTheSame(oldItem, newItem)

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            oldItem == newItem
    }
}
