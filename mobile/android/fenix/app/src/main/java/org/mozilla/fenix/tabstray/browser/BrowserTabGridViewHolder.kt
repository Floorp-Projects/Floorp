/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatImageButton
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import kotlin.math.max
import kotlinx.android.synthetic.main.tab_tray_grid_item.view.tab_tray_grid_item
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * A RecyclerView ViewHolder implementation for "tab" items with grid layout.
 */
class BrowserTabGridViewHolder(
    imageLoader: ImageLoader,
    override val browserTrayInteractor: BrowserTrayInteractor,
    store: TabsTrayStore,
    selectionHolder: SelectionHolder<Tab>? = null,
    itemView: View
) : AbstractBrowserTabViewHolder(itemView, imageLoader, store, selectionHolder) {

    private val closeButton: AppCompatImageButton = itemView.findViewById(R.id.mozac_browser_tabstray_close)

    override val thumbnailSize: Int
        get() = max(
            itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_thumbnail_height),
            itemView.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_thumbnail_width)
        )

    override fun updateSelectedTabIndicator(showAsSelected: Boolean) {
        itemView.tab_tray_grid_item.background = if (showAsSelected) {
            AppCompatResources.getDrawable(itemView.context, R.drawable.tab_tray_grid_item_selected_border)
        } else {
            null
        }
        return
    }

    override fun bind(
        tab: Tab,
        isSelected: Boolean,
        styling: TabsTrayStyling,
        observable: Observable<TabsTray.Observer>
    ) {
        super.bind(tab, isSelected, styling, observable)

        closeButton.increaseTapArea(GRID_ITEM_CLOSE_BUTTON_EXTRA_DPS)
    }
}
