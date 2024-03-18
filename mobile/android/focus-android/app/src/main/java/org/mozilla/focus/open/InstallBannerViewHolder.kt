/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.open

import android.view.View
import android.widget.ImageView
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.focus.R
import org.mozilla.focus.activity.InstallFirefoxActivity.Companion.open

/**
 * View holder for install Firefox item in the [OpenWithFragment] list.
 */
class InstallBannerViewHolder(
    itemView: View,
) : RecyclerView.ViewHolder(itemView), View.OnClickListener {
    private val iconView: ImageView

    init {
        iconView = itemView.findViewById(R.id.icon)
        itemView.setOnClickListener(this)
    }

    /**
     * Binds the [InstallBannerViewHolder] item.
     */
    fun bind(store: AppAdapter.App) {
        iconView.setImageDrawable(store.loadIcon())
    }

    override fun onClick(view: View) {
        open(view.context)
    }

    companion object {
        const val LAYOUT_ID = R.layout.item_install_banner
    }
}
