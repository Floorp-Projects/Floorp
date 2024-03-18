/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.ui.widgets.WidgetSiteItemView
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView

class TabHistoryViewHolder(
    private val view: WidgetSiteItemView,
    private val interactor: TabHistoryViewInteractor,
    private val icons: BrowserIcons = view.context.components.core.icons,
) : RecyclerView.ViewHolder(view) {

    private lateinit var item: TabHistoryItem

    init {
        view.setOnClickListener { interactor.goToHistoryItem(item) }
    }

    fun bind(item: TabHistoryItem) {
        this.item = item

        view.setText(label = item.title, caption = item.url)
        icons.loadIntoView(view.iconView, item.url)

        if (item.isSelected) {
            view.setBackgroundColor(
                view.context.getColorFromAttr(R.attr.layerNonOpaque),
            )
        } else {
            view.background = null
        }
    }
}
