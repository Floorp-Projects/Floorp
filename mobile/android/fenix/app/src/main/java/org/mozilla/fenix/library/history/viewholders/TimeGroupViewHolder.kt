/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryListHeaderBinding
import org.mozilla.fenix.library.history.HistoryAdapter
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.library.history.HistoryViewItem

/**
 * A view representing a Header in the history and synced history lists.
 * [HistoryAdapter] is responsible for creating and populating the view with data.
 *
 * @param view that is passed down to the parent's constructor.
 * @param onClickListener Invokes on a click event on the viewHolder.
 */
class TimeGroupViewHolder(
    view: View,
    private val onClickListener: (HistoryItemTimeGroup, Boolean) -> Unit
) : RecyclerView.ViewHolder(view) {

    private val binding = HistoryListHeaderBinding.bind(view)
    private lateinit var item: HistoryViewItem.TimeGroupHeader

    init {
        binding.root.setOnClickListener {
            onClickListener.invoke(item.timeGroup, item.collapsed)
        }
    }

    /**
     * Binds data to the view.
     *
     * @param item Data associated with the view.
     */
    fun bind(item: HistoryViewItem.TimeGroupHeader) {
        binding.headerTitle.text = item.title
        binding.chevron.isActivated = !item.collapsed
        this.item = item
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_list_header
    }
}
