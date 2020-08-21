/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.download_list_item.view.*
import kotlinx.android.synthetic.main.library_site_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.library.SelectionHolder
import org.mozilla.fenix.library.downloads.DownloadInteractor
import org.mozilla.fenix.library.downloads.DownloadItem
import mozilla.components.feature.downloads.toMegabyteString
import org.mozilla.fenix.ext.getIcon

class DownloadsListItemViewHolder(
    view: View,
    private val downloadInteractor: DownloadInteractor,
    private val selectionHolder: SelectionHolder<DownloadItem>
) : RecyclerView.ViewHolder(view) {

    private var item: DownloadItem? = null

    fun bind(
        item: DownloadItem
    ) {
        itemView.download_layout.visibility = View.VISIBLE
        itemView.download_layout.titleView.text = item.fileName
        itemView.download_layout.urlView.text = item.size.toLong().toMegabyteString()

        itemView.download_layout.setSelectionInteractor(item, selectionHolder, downloadInteractor)
        itemView.download_layout.changeSelected(item in selectionHolder.selectedItems)

        itemView.overflow_menu.hideAndDisable()
        itemView.favicon.setImageResource(item.getIcon())
        itemView.favicon.isClickable = false

        this.item = item
    }

    companion object {
        const val LAYOUT_ID = R.layout.download_list_item
    }
}
