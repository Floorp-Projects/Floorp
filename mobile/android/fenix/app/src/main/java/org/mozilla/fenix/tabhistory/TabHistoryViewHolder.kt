/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.view.View
import androidx.core.text.bold
import androidx.core.text.buildSpannedString
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.history_list_item.view.*

class TabHistoryViewHolder(
    private val view: View,
    private val interactor: TabHistoryViewInteractor
) : RecyclerView.ViewHolder(view) {

    fun bind(item: TabHistoryItem) {
        view.history_layout.overflowView.isVisible = false
        view.history_layout.urlView.text = item.url
        view.history_layout.loadFavicon(item.url)

        view.history_layout.titleView.text = if (item.isSelected) {
            buildSpannedString {
                bold { append(item.title) }
            }
        } else {
            item.title
        }

        view.setOnClickListener { interactor.goToHistoryItem(item) }
    }
}
