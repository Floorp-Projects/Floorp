/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import mozilla.components.concept.base.images.ImageLoader
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import kotlin.math.max

/**
 * A RecyclerView ViewHolder implementation for "tab" items with list layout.
 */
class TabsTrayListViewHolder(
    parent: ViewGroup,
    imageLoader: ImageLoader,
    browserTrayInteractor: BrowserTrayInteractor? = null,
    itemView: View =
        LayoutInflater.from(parent.context).inflate(R.layout.tab_tray_item, parent, false),
    thumbnailSize: Int =
        max(
            itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_height),
            itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_width)
        )
) : TabsTrayViewHolder(itemView, imageLoader, thumbnailSize, browserTrayInteractor) {

    override fun updateSelectedTabIndicator(showAsSelected: Boolean) {
        val color = if (showAsSelected) {
            R.color.tab_tray_item_selected_background_normal_theme
        } else {
            R.color.tab_tray_item_background_normal_theme
        }
        itemView.setBackgroundColor(
            ContextCompat.getColor(
                itemView.context,
                color
            )
        )
    }
}
