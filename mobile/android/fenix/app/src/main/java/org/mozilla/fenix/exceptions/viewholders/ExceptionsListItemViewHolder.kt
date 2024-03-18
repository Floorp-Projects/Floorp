/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.ui.widgets.WidgetSiteItemView
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsInteractor
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView

/**
 * View holder for a single website that is exempted from Tracking Protection or Logins.
 */
class ExceptionsListItemViewHolder<T : Any>(
    private val view: WidgetSiteItemView,
    private val interactor: ExceptionsInteractor<T>,
    private val icons: BrowserIcons = view.context.components.core.icons,
) : RecyclerView.ViewHolder(view) {

    private lateinit var item: T

    init {
        view.setSecondaryButton(
            icon = R.drawable.ic_close,
            contentDescription = R.string.history_delete_item,
        ) {
            interactor.onDeleteOne(item)
        }
    }

    fun bind(item: T, url: String) {
        this.item = item
        view.setText(label = url, caption = null)
        icons.loadIntoView(view.iconView, url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.site_list_item
    }
}
