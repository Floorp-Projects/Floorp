/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.tabtray.SaveToCollectionsButtonAdapter.Item
import org.mozilla.fenix.tabtray.SaveToCollectionsButtonAdapter.ViewHolder

/**
 * An adapter to display a single 'Save to Collections' button that can be used to display between
 * multiple [RecyclerView.Adapter] in one [RecyclerView].
 */
class SaveToCollectionsButtonAdapter(
    private val interactor: TabTrayInteractor,
    private val isPrivate: Boolean = false
) : ListAdapter<Item, ViewHolder>(DiffCallback) {

    init {
        submitList(listOf(Item))
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return ViewHolder(itemView, interactor)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int, payloads: MutableList<Any>) {
        if (payloads.isNullOrEmpty()) {
            onBindViewHolder(holder, position)
            return
        }

        when (val change = payloads[0]) {
            is TabTrayView.TabChange -> {
                holder.itemView.isVisible = change == TabTrayView.TabChange.NORMAL
            }
            is MultiselectModeChange -> {
                holder.itemView.isVisible = change == MultiselectModeChange.NORMAL
            }
        }
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.itemView.isVisible = !isPrivate &&
                interactor.onModeRequested() is TabTrayDialogFragmentState.Mode.Normal
    }

    override fun getItemViewType(position: Int): Int {
        return ViewHolder.LAYOUT_ID
    }

    private object DiffCallback : DiffUtil.ItemCallback<Item>() {
        override fun areItemsTheSame(oldItem: Item, newItem: Item) = true

        override fun areContentsTheSame(oldItem: Item, newItem: Item) = true
    }

    enum class MultiselectModeChange {
        MULTISELECT, NORMAL
    }

    /**
     * An object to identify the data type.
     */
    object Item

    class ViewHolder(
        itemView: View,
        private val interactor: TabTrayInteractor
    ) : RecyclerView.ViewHolder(itemView), View.OnClickListener {
        init {
            itemView.setOnClickListener(this)
        }

        override fun onClick(v: View?) {
            interactor.onEnterMultiselect()
        }

        companion object {
            const val LAYOUT_ID = R.layout.tabs_tray_save_to_collections_item
        }
    }
}
