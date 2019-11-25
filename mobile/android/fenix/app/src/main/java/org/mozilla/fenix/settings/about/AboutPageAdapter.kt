/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.settings.about.viewholders.AboutItemViewHolder

class AboutPageAdapter(private val listener: AboutPageListener) : RecyclerView.Adapter<AboutItemViewHolder>() {

    @VisibleForTesting
    var aboutList: List<AboutPageItem>? = null

    fun updateData(items: List<AboutPageItem>) {
        this.aboutList = items
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AboutItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AboutItemViewHolder.LAYOUT_ID, parent, false)

        return AboutItemViewHolder(view, listener)
    }

    override fun getItemCount(): Int = aboutList?.size ?: 0

    override fun onBindViewHolder(holder: AboutItemViewHolder, position: Int) {
        (aboutList?.get(position) as AboutPageItem.Item).also {
            holder.bind(it)
        }
    }
}

interface AboutPageListener {
    fun onAboutItemClicked(item: AboutItem)
}
