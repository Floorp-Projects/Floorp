/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.focus.databinding.ItemSessionBinding

/**
 * Adapter implementation to show a list of active tabs.
 */
class TabsAdapter internal constructor(
    private val tabList: List<TabSessionState>,
    private val isCurrentSession: (TabSessionState) -> Boolean,
    private val selectSession: (TabSessionState) -> Unit,
    private val closeSession: (TabSessionState) -> Unit,
) : RecyclerView.Adapter<TabViewHolder>() {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        val binding =
            ItemSessionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return TabViewHolder(binding)
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        val currentItem = tabList[position]
        holder.bind(
            currentItem,
            isCurrentSession = isCurrentSession.invoke(currentItem),
            selectSession = selectSession,
            closeSession = closeSession,
        )
    }

    override fun getItemCount() = tabList.size
}
