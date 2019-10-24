/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.logins

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView

private sealed class AdapterItem {
    data class Item(val item: SavedLoginsItem) : AdapterItem()
}

private class SavedLoginsList(savedLogins: List<SavedLoginsItem>) {
    val items: List<AdapterItem> = savedLogins.map { AdapterItem.Item(it) }
}

class SavedLoginsAdapter(
    private val interactor: SavedLoginsInteractor
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    private var savedLoginsList: SavedLoginsList = SavedLoginsList(emptyList())

    fun updateData(items: List<SavedLoginsItem>) {
        this.savedLoginsList = SavedLoginsList(items)
        notifyDataSetChanged()
    }

    override fun getItemCount(): Int = savedLoginsList.items.size

    override fun getItemViewType(position: Int): Int {
        return when (savedLoginsList.items[position]) {
            is AdapterItem.Item -> SavedLoginsListItemViewHolder.LAYOUT_ID
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            SavedLoginsListItemViewHolder.LAYOUT_ID -> SavedLoginsListItemViewHolder(
                view,
                interactor
            )
            else -> throw IllegalStateException()
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is SavedLoginsListItemViewHolder -> (savedLoginsList.items[position] as AdapterItem.Item).also {
                holder.bind(it.item)
            }
        }
    }
}
