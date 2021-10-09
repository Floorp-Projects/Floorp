/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.library.downloads.viewholders.DownloadsListItemViewHolder

class DownloadAdapter(
    private val downloadInteractor: DownloadInteractor
) : RecyclerView.Adapter<DownloadsListItemViewHolder>(), SelectionHolder<DownloadItem> {
    private var downloads: List<DownloadItem> = listOf()
    private var mode: DownloadFragmentState.Mode = DownloadFragmentState.Mode.Normal
    override val selectedItems get() = mode.selectedItems
    var pendingDeletionIds = emptySet<String>()

    override fun getItemCount(): Int = downloads.size
    override fun getItemViewType(position: Int): Int = DownloadsListItemViewHolder.LAYOUT_ID

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DownloadsListItemViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return DownloadsListItemViewHolder(view, downloadInteractor, this)
    }

    fun updateMode(mode: DownloadFragmentState.Mode) {
        this.mode = mode
        // Update the delete button alpha that the first item holds
        if (itemCount > 0) notifyItemChanged(0)
    }

    override fun onBindViewHolder(holder: DownloadsListItemViewHolder, position: Int) {
        val current = downloads[position]
        val isPendingDeletion = pendingDeletionIds.contains(current.id)
        holder.bind(downloads[position], isPendingDeletion)
    }

    fun updateDownloads(downloads: List<DownloadItem>) {
        this.downloads = downloads
        notifyDataSetChanged()
    }

    fun updatePendingDeletionIds(pendingDeletionIds: Set<String>) {
        this.pendingDeletionIds = pendingDeletionIds
    }

    companion object {
        private val downloadDiffCallback = object : DiffUtil.ItemCallback<DownloadItem>() {
            override fun areItemsTheSame(oldItem: DownloadItem, newItem: DownloadItem): Boolean {
                return oldItem == newItem
            }

            override fun areContentsTheSame(oldItem: DownloadItem, newItem: DownloadItem): Boolean {
                return oldItem == newItem
            }

            override fun getChangePayload(oldItem: DownloadItem, newItem: DownloadItem): Any? {
                return newItem
            }
        }
    }
}
