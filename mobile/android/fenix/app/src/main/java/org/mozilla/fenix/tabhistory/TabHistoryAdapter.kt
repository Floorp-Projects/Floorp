/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.ui.widgets.WidgetSiteItemView
import org.mozilla.fenix.R

data class TabHistoryItem(
    val title: String,
    val url: String,
    val index: Int,
    val isSelected: Boolean,
)

class TabHistoryAdapter(
    private val interactor: TabHistoryViewInteractor,
) : ListAdapter<TabHistoryItem, TabHistoryViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabHistoryViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.site_list_item, parent, false) as WidgetSiteItemView
        return TabHistoryViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: TabHistoryViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    internal object DiffCallback : DiffUtil.ItemCallback<TabHistoryItem>() {
        override fun areItemsTheSame(oldItem: TabHistoryItem, newItem: TabHistoryItem) =
            oldItem.url == newItem.url

        override fun areContentsTheSame(oldItem: TabHistoryItem, newItem: TabHistoryItem) =
            oldItem == newItem
    }
}
