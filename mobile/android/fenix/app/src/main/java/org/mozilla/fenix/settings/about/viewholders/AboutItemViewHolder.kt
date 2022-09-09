/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.AboutListItemBinding
import org.mozilla.fenix.settings.about.AboutPageItem
import org.mozilla.fenix.settings.about.AboutPageListener

class AboutItemViewHolder(
    view: View,
    listener: AboutPageListener,
) : RecyclerView.ViewHolder(view) {

    private lateinit var item: AboutPageItem
    val binding = AboutListItemBinding.bind(view)

    init {
        itemView.setOnClickListener {
            listener.onAboutItemClicked(item.type)
        }
    }

    fun bind(item: AboutPageItem) {
        this.item = item
        binding.aboutItemTitle.text = item.title
    }

    companion object {
        const val LAYOUT_ID = R.layout.about_list_item
    }
}
