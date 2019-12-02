/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.logins

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter

private sealed class AdapterItem {
    data class Item(val item: SavedLoginsItem) : AdapterItem()
}

class SavedLoginsAdapter(
    private val interactor: SavedLoginsInteractor
) : ListAdapter<SavedLoginsItem, SavedLoginsListItemViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): SavedLoginsListItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(SavedLoginsListItemViewHolder.LAYOUT_ID, parent, false)
        return SavedLoginsListItemViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: SavedLoginsListItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<SavedLoginsItem>() {
        override fun areItemsTheSame(oldItem: SavedLoginsItem, newItem: SavedLoginsItem) =
            oldItem.url == newItem.url

        override fun areContentsTheSame(oldItem: SavedLoginsItem, newItem: SavedLoginsItem) =
            oldItem == newItem
    }
}
