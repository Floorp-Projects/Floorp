/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.listadapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.share_tab_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.share.ShareTab

class ShareTabsAdapter :
    ListAdapter<ShareTab, ShareTabsAdapter.ShareTabViewHolder>(ShareTabDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = ShareTabViewHolder(
        LayoutInflater.from(parent.context)
            .inflate(R.layout.share_tab_item, parent, false)
    )

    override fun onBindViewHolder(holder: ShareTabViewHolder, position: Int) =
        holder.bind(getItem(position))

    fun setTabs(tabs: List<ShareTab>) {
        submitList(tabs.toMutableList())
    }

    inner class ShareTabViewHolder(
        itemView: View
    ) : RecyclerView.ViewHolder(itemView) {

        fun bind(item: ShareTab) = with(itemView) {
            context.components.core.icons.loadIntoView(itemView.share_tab_favicon, item.url)
            itemView.share_tab_title.text = item.title
            itemView.share_tab_url.text = item.url
        }
    }

    private class ShareTabDiffCallback : DiffUtil.ItemCallback<ShareTab>() {
        override fun areItemsTheSame(
            oldItem: ShareTab,
            newItem: ShareTab
        ): Boolean {
            return oldItem.url == newItem.url
        }

        override fun areContentsTheSame(
            oldItem: ShareTab,
            newItem: ShareTab
        ): Boolean {
            return oldItem.url == newItem.url && oldItem.title == newItem.title
        }
    }
}
