/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor
import org.mozilla.fenix.home.recentbookmarks.view.RecentBookmarkItemViewHolder

/**
 * Adapter for binding individual bookmark items for the homescreen.
 *
 * @param interactor The [RecentBookmarksInteractor] to be passed into the view.
 */
class RecentBookmarksItemAdapter(
    private val interactor: RecentBookmarksInteractor
) : ListAdapter<BookmarkNode, RecentBookmarkItemViewHolder>(RecentBookmarkItemDiffCallback) {

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): RecentBookmarkItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(RecentBookmarkItemViewHolder.LAYOUT_ID, parent, false)
        return RecentBookmarkItemViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: RecentBookmarkItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    internal object RecentBookmarkItemDiffCallback : DiffUtil.ItemCallback<BookmarkNode>() {
        override fun areItemsTheSame(oldItem: BookmarkNode, newItem: BookmarkNode) =
            oldItem.guid == newItem.guid

        override fun areContentsTheSame(oldItem: BookmarkNode, newItem: BookmarkNode) =
            oldItem == newItem
    }
}
