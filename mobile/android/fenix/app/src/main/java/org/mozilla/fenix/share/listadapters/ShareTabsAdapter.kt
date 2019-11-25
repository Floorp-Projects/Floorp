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
import mozilla.components.concept.engine.prompt.ShareData
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView

/**
 * Adapter for a list of tabs to be shared.
 */
class ShareTabsAdapter :
    ListAdapter<ShareData, ShareTabsAdapter.ShareTabViewHolder>(ShareTabDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = ShareTabViewHolder(
        LayoutInflater.from(parent.context)
            .inflate(R.layout.share_tab_item, parent, false)
    )

    override fun onBindViewHolder(holder: ShareTabViewHolder, position: Int) =
        holder.bind(getItem(position))

    class ShareTabViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {

        fun bind(item: ShareData) = with(itemView) {
            context.components.core.icons.loadIntoView(itemView.share_tab_favicon, item.url.orEmpty())
            itemView.share_tab_title.text = item.title
            itemView.share_tab_url.text = item.url
        }
    }

    private object ShareTabDiffCallback : DiffUtil.ItemCallback<ShareData>() {
        override fun areItemsTheSame(oldItem: ShareData, newItem: ShareData) =
            oldItem.url == newItem.url

        override fun areContentsTheSame(oldItem: ShareData, newItem: ShareData) =
            oldItem == newItem
    }
}
