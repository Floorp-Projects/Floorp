/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import org.mozilla.fenix.settings.about.viewholders.AboutItemViewHolder

class AboutPageAdapter(private val listener: AboutPageListener) :
    ListAdapter<AboutPageItem, AboutItemViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AboutItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AboutItemViewHolder.LAYOUT_ID, parent, false)
        return AboutItemViewHolder(view, listener)
    }

    override fun onBindViewHolder(holder: AboutItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<AboutPageItem>() {

        override fun areItemsTheSame(oldItem: AboutPageItem, newItem: AboutPageItem) =
            oldItem.title == newItem.title

        override fun areContentsTheSame(oldItem: AboutPageItem, newItem: AboutPageItem) =
            oldItem == newItem
    }
}

interface AboutPageListener {
    fun onAboutItemClicked(item: AboutItem)
}
