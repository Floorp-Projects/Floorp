/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.browser.state.state.recover.RecoverableTab

class RecentlyClosedAdapter(
    private val interactor: RecentlyClosedFragmentInteractor
) : ListAdapter<RecoverableTab, RecentlyClosedItemViewHolder>(DiffCallback) {
    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): RecentlyClosedItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(RecentlyClosedItemViewHolder.LAYOUT_ID, parent, false)
        return RecentlyClosedItemViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: RecentlyClosedItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<RecoverableTab>() {
        override fun areItemsTheSame(oldItem: RecoverableTab, newItem: RecoverableTab) =
            oldItem.id == newItem.id

        override fun areContentsTheSame(oldItem: RecoverableTab, newItem: RecoverableTab) =
            oldItem.id == newItem.id
    }
}
