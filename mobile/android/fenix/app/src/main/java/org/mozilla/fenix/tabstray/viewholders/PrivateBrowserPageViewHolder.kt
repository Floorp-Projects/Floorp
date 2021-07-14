/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.defaultBrowserLayoutColumns

/**
 * View holder for the private tabs tray list.
 */
class PrivateBrowserPageViewHolder(
    containerView: View,
    store: TabsTrayStore,
    interactor: TabsTrayInteractor,
    currentTabIndex: Int
) : AbstractBrowserPageViewHolder(
    containerView,
    store,
    interactor,
    currentTabIndex
) {

    override val emptyStringText: String
        get() = itemView.resources.getString(R.string.no_private_tabs_description)

    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>
    ) {
        val context = containerView.context
        val columns = context.defaultBrowserLayoutColumns
        val manager = GridLayoutManager(context, columns)

        super.bind(adapter, manager)
    }

    companion object {
        const val LAYOUT_ID = R.layout.private_browser_tray_list
    }
}
