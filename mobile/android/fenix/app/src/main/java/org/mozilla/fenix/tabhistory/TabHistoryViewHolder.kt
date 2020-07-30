/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.view.View
import androidx.core.text.bold
import androidx.core.text.buildSpannedString
import kotlinx.android.synthetic.main.history_list_item.*
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.utils.view.ViewHolder

class TabHistoryViewHolder(
    view: View,
    private val interactor: TabHistoryViewInteractor
) : ViewHolder(view) {

    private lateinit var item: TabHistoryItem

    init {
        itemView.setOnClickListener { interactor.goToHistoryItem(item) }
    }

    fun bind(item: TabHistoryItem) {
        this.item = item

        history_layout.displayAs(LibrarySiteItemView.ItemType.SITE)
        history_layout.urlView.text = item.url
        history_layout.loadFavicon(item.url)

        history_layout.titleView.text = if (item.isSelected) {
            buildSpannedString {
                bold { append(item.title) }
            }
        } else {
            item.title
        }
    }
}
