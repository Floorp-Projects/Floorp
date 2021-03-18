/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.tabstray.BrowserTabViewHolder.Companion.LAYOUT_ID_NORMAL_TAB
import org.mozilla.fenix.tabstray.BrowserTabViewHolder.Companion.LAYOUT_ID_PRIVATE_TAB
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor

class TrayPagerAdapter(
    val context: Context,
    val interactor: TabsTrayInteractor,
    val browserInteractor: BrowserTrayInteractor
) : RecyclerView.Adapter<TrayViewHolder>() {

    private val normalAdapter by lazy { BrowserTabsAdapter(context, browserInteractor) }
    private val privateAdapter by lazy { BrowserTabsAdapter(context, browserInteractor) }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TrayViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            LAYOUT_ID_NORMAL_TAB -> BrowserTabViewHolder(itemView, interactor)
            LAYOUT_ID_PRIVATE_TAB -> BrowserTabViewHolder(itemView, interactor)
            else -> throw IllegalStateException("Unknown viewType.")
        }
    }

    override fun onBindViewHolder(viewHolder: TrayViewHolder, position: Int) {
        val adapter = when (position) {
            POSITION_NORMAL_TABS -> normalAdapter
            POSITION_PRIVATE_TABS -> privateAdapter
            else -> throw IllegalStateException("View type does not exist.")
        }

        viewHolder.bind(adapter, GridLayoutManager(context, 1))
    }

    override fun getItemViewType(position: Int): Int {
        return when (position) {
            POSITION_NORMAL_TABS -> LAYOUT_ID_NORMAL_TAB
            POSITION_PRIVATE_TABS -> LAYOUT_ID_PRIVATE_TAB
            else -> throw IllegalStateException("Unknown position.")
        }
    }

    override fun getItemCount(): Int = TRAY_TABS_COUNT

    companion object {
        const val TRAY_TABS_COUNT = 2

        const val POSITION_NORMAL_TABS = 0
        const val POSITION_PRIVATE_TABS = 1
    }
}
