/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.component_sync_tabs_tray_layout.*
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayStore

class SyncedTabsPageViewHolder(
    containerView: View,
    private val tabsTrayStore: TabsTrayStore
) : AbstractPageViewHolder(containerView) {

    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager
    ) {
        synced_tabs_list.layoutManager = layoutManager
        synced_tabs_list.adapter = adapter

        synced_tabs_tray_layout.tabsTrayStore = tabsTrayStore
    }

    companion object {
        const val LAYOUT_ID = R.layout.component_sync_tabs_tray_layout
    }
}
