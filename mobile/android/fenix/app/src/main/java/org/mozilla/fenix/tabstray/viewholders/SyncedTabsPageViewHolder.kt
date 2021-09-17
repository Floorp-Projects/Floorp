/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentSyncTabsTrayLayoutBinding
import org.mozilla.fenix.tabstray.TabsTrayStore

class SyncedTabsPageViewHolder(
    containerView: View,
    private val tabsTrayStore: TabsTrayStore
) : AbstractPageViewHolder(containerView) {

    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>
    ) {
        val binding = ComponentSyncTabsTrayLayoutBinding.bind(containerView)

        binding.syncedTabsList.layoutManager = GridLayoutManager(containerView.context, 1)
        binding.syncedTabsList.adapter = adapter

        binding.syncedTabsTrayLayout.tabsTrayStore = tabsTrayStore
    }

    override fun detachedFromWindow() = Unit // no-op
    override fun attachedToWindow() = Unit // no-op

    companion object {
        const val LAYOUT_ID = R.layout.component_sync_tabs_tray_layout
    }
}
