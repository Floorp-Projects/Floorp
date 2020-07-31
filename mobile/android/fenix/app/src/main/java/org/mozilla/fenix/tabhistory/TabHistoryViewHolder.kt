/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.view.View
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.tab_history_list_item.*
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.utils.view.ViewHolder

class TabHistoryViewHolder(
    view: View,
    private val interactor: TabHistoryViewInteractor
) : ViewHolder(view) {

    private lateinit var item: TabHistoryItem

    init {
        history_layout.setOnClickListener { interactor.goToHistoryItem(item) }
    }

    fun bind(item: TabHistoryItem) {
        this.item = item

        history_layout.displayAs(LibrarySiteItemView.ItemType.SITE)
        history_layout.overflowView.isVisible = false
        history_layout.titleView.text = item.title
        history_layout.urlView.text = item.url
        history_layout.loadFavicon(item.url)

        if (item.isSelected) {
            history_layout.setBackgroundColor(history_layout.context.getColorFromAttr(R.attr.tabHistoryItemSelectedBackground))
        } else {
            history_layout.background = null
        }
    }
}
