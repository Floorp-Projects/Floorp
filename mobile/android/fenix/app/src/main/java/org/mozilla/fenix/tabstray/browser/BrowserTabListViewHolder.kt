/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import androidx.core.content.ContextCompat
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.tabstray.Tab
import org.mozilla.fenix.R
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore
import kotlin.math.max

/**
 * A RecyclerView ViewHolder implementation for "tab" items with list layout.
 *
 * @param imageLoader [ImageLoader] used to load tab thumbnails.
 * @param browserTrayInteractor [BrowserTrayInteractor] handling tabs interactions in a tab tray.
 * @param store [TabsTrayStore] containing the complete state of tabs tray and methods to update that.
 * @param selectionHolder [SelectionHolder]<[Tab]> for helping with selecting any number of displayed [Tab]s.
 * @param itemView [View] that displays a "tab".
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
*/
class BrowserTabListViewHolder(
    imageLoader: ImageLoader,
    override val browserTrayInteractor: BrowserTrayInteractor,
    store: TabsTrayStore,
    selectionHolder: SelectionHolder<Tab>? = null,
    itemView: View,
    featureName: String
) : AbstractBrowserTabViewHolder(itemView, imageLoader, store, selectionHolder, featureName) {
    override val thumbnailSize: Int
        get() = max(
            itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_height),
            itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_list_item_thumbnail_width)
        )

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
