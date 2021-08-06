/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads.viewholders

import android.view.View
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.downloads.toMegabyteOrKilobyteString
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.DownloadListItemBinding
import org.mozilla.fenix.databinding.LibrarySiteItemBinding
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.library.downloads.DownloadInteractor
import org.mozilla.fenix.library.downloads.DownloadItem
import org.mozilla.fenix.ext.getIcon
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.downloads.DownloadFragmentState
import org.mozilla.fenix.library.downloads.DownloadItemMenu

class DownloadsListItemViewHolder(
    view: View,
    private val downloadInteractor: DownloadInteractor,
    private val selectionHolder: SelectionHolder<DownloadItem>
) : RecyclerView.ViewHolder(view) {

    private var item: DownloadItem? = null
    private val binding = DownloadListItemBinding.bind(view)
    private val librarySiteItemBinding = LibrarySiteItemBinding.bind(binding.downloadLayout)

    init {
        setupMenu()

        binding.deleteDownloadsButton.setOnClickListener {
            val selected = selectionHolder.selectedItems
            if (selected.isEmpty()) {
                downloadInteractor.onDeleteAll()
            } else {
                downloadInteractor.onDeleteSome(selected)
            }
        }
    }

    fun bind(
        item: DownloadItem,
        mode: DownloadFragmentState.Mode,
        isPendingDeletion: Boolean = false
    ) {
        binding.downloadLayout.visibility = if (isPendingDeletion) {
            View.GONE
        } else {
            View.VISIBLE
        }
        binding.downloadLayout.titleView.text = item.fileName
        binding.downloadLayout.urlView.text = item.size.toLong().toMegabyteOrKilobyteString()

        toggleTopContent(false, mode == DownloadFragmentState.Mode.Normal)

        binding.downloadLayout.setSelectionInteractor(item, selectionHolder, downloadInteractor)
        binding.downloadLayout.changeSelected(item in selectionHolder.selectedItems)

        librarySiteItemBinding.favicon.setImageResource(item.getIcon())

        librarySiteItemBinding.overflowMenu.setImageResource(R.drawable.ic_delete)

        librarySiteItemBinding.overflowMenu.showAndEnable()

        librarySiteItemBinding.overflowMenu.setOnClickListener {
            downloadInteractor.onDeleteSome(setOf(item))
        }

        this.item = item
    }

    private fun toggleTopContent(
        showTopContent: Boolean,
        isNormalMode: Boolean
    ) {
        binding.deleteDownloadsButton.isVisible = showTopContent

        if (showTopContent) {
            binding.deleteDownloadsButton.run {
                if (isNormalMode) {
                    isEnabled = true
                    alpha = 1f
                } else {
                    isEnabled = false
                    alpha = DELETE_BUTTON_DISABLED_ALPHA
                }
            }
        }
    }

    private fun setupMenu() {
        val downloadMenu = DownloadItemMenu(itemView.context) {
            val item = this.item ?: return@DownloadItemMenu

            if (it == DownloadItemMenu.Item.Delete) {
                downloadInteractor.onDeleteSome(setOf(item))
            }
        }
        binding.downloadLayout.attachMenu(downloadMenu.menuController)
    }

    companion object {
        const val DELETE_BUTTON_DISABLED_ALPHA = 0.4f
        const val LAYOUT_ID = R.layout.download_list_item
    }
}
