/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

/**
 * Adapter implementation to show a list of active tabs and an "erase" button at the end.
 */
class TabsAdapter internal constructor(
    private val fragment: TabSheetFragment,
    private var tabs: List<TabSessionState> = emptyList()
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)

        return when (viewType) {
            EraseViewHolder.LAYOUT_ID -> EraseViewHolder(
                fragment,
                inflater.inflate(EraseViewHolder.LAYOUT_ID, parent, false)
            )
            TabViewHolder.LAYOUT_ID -> TabViewHolder(
                fragment,
                inflater.inflate(TabViewHolder.LAYOUT_ID, parent, false) as TextView
            )
            else -> throw IllegalStateException("Unknown viewType")
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder.itemViewType) {
            EraseViewHolder.LAYOUT_ID -> { /* Nothing to do */ }
            TabViewHolder.LAYOUT_ID -> (holder as TabViewHolder).bind(tabs[position])
            else -> throw IllegalStateException("Unknown viewType")
        }
    }

    override fun getItemViewType(position: Int): Int {
        return if (isErasePosition(position)) {
            EraseViewHolder.LAYOUT_ID
        } else {
            TabViewHolder.LAYOUT_ID
        }
    }

    private fun isErasePosition(position: Int): Boolean {
        return position == tabs.size
    }

    override fun getItemCount(): Int {
        return tabs.size + 1
    }

    suspend fun onFlow(flow: Flow<BrowserState>) {
        flow.ifAnyChanged { state -> arrayOf(state.privateTabs.size, state.selectedTabId) }
            .collect { state -> onUpdate(state.privateTabs) }
    }

    private fun onUpdate(tabs: List<TabSessionState>) {
        this.tabs = tabs
        notifyDataSetChanged()
    }
}
