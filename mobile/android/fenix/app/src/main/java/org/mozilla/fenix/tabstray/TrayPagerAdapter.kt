/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.viewholders.AbstractTrayViewHolder
import org.mozilla.fenix.tabstray.viewholders.NormalBrowserTabViewHolder
import org.mozilla.fenix.tabstray.viewholders.PrivateBrowserTabViewHolder
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.sync.SyncedTabsAdapter
import org.mozilla.fenix.tabstray.viewholders.SyncedTabViewHolder

class TrayPagerAdapter(
    val context: Context,
    val interactor: TabsTrayInteractor,
    val browserInteractor: BrowserTrayInteractor,
    val syncedTabsInteractor: SyncedTabsView.Listener
) : RecyclerView.Adapter<AbstractTrayViewHolder>() {

    private val normalAdapter by lazy { BrowserTabsAdapter(context, browserInteractor) }
    private val privateAdapter by lazy { BrowserTabsAdapter(context, browserInteractor) }
    private val syncedTabsAdapter by lazy { SyncedTabsAdapter(syncedTabsInteractor) }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AbstractTrayViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            NormalBrowserTabViewHolder.LAYOUT_ID -> NormalBrowserTabViewHolder(
                itemView,
                interactor
            )
            PrivateBrowserTabViewHolder.LAYOUT_ID -> PrivateBrowserTabViewHolder(
                itemView,
                interactor
            )
            SyncedTabViewHolder.LAYOUT_ID -> SyncedTabViewHolder(
                itemView,
                syncedTabsInteractor
            )
            else -> throw IllegalStateException("Unknown viewType.")
        }
    }

    override fun onBindViewHolder(viewHolder: AbstractTrayViewHolder, position: Int) {
        val adapter = when (position) {
            POSITION_NORMAL_TABS -> normalAdapter
            POSITION_PRIVATE_TABS -> privateAdapter
            POSITION_SYNCED_TABS -> syncedTabsAdapter
            else -> throw IllegalStateException("View type does not exist.")
        }

        viewHolder.bind(adapter, GridLayoutManager(context, 1))
    }

    override fun getItemViewType(position: Int): Int {
        return when (position) {
            POSITION_NORMAL_TABS -> NormalBrowserTabViewHolder.LAYOUT_ID
            POSITION_PRIVATE_TABS -> PrivateBrowserTabViewHolder.LAYOUT_ID
            POSITION_SYNCED_TABS -> SyncedTabViewHolder.LAYOUT_ID
            else -> throw IllegalStateException("Unknown position.")
        }
    }

    override fun getItemCount(): Int = TRAY_TABS_COUNT

    companion object {
        const val TRAY_TABS_COUNT = 3

        const val POSITION_NORMAL_TABS = 0
        const val POSITION_PRIVATE_TABS = 1
        const val POSITION_SYNCED_TABS = 2
    }
}
