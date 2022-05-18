/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.TabGroupItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.TrayPagerAdapter

/**
 * A RecyclerView ViewHolder implementation for tab group items.
 *
 * @param itemView [View] that displays a "tab".
 * @param orientation [Int] orientation of the items.  Horizontal for grid layout, vertical for list layout
 * @param interactor the [BrowserTrayInteractor] for tab interactions.
 * @param store the [TabsTrayStore] instance.
 * @param selectionHolder the store that holds the currently selected tabs.
 * @param viewLifecycleOwner [LifecycleOwner] life cycle owner for the view.
 */
class TabGroupViewHolder(
    itemView: View,
    val orientation: Int,
    val interactor: BrowserTrayInteractor,
    val store: TabsTrayStore,
    val selectionHolder: SelectionHolder<TabSessionState>? = null,
    private val viewLifecycleOwner: LifecycleOwner
) : RecyclerView.ViewHolder(itemView) {
    private val binding = TabGroupItemBinding.bind(itemView)

    private lateinit var groupListAdapter: TabGroupListAdapter

    fun bind(
        tabGroup: TabGroup,
    ) {
        val selectedTabId = itemView.context.components.core.store.state.selectedTabId
        val selectedIndex = tabGroup.tabIds.indexOfFirst { it == selectedTabId }

        binding.tabGroupTitle.text = tabGroup.id
        binding.tabGroupList.apply {
            layoutManager = LinearLayoutManager(itemView.context, orientation, false)
            groupListAdapter = TabGroupListAdapter(
                context = itemView.context,
                interactor = interactor,
                store = store,
                selectionHolder = selectionHolder,
                featureName = TrayPagerAdapter.TAB_GROUP_FEATURE_NAME,
                viewLifecycleOwner
            )

            adapter = groupListAdapter

            val tabGroupTabs = itemView.context.components.core.store.state.normalTabs.filter {
                tabGroup.tabIds.contains(it.id)
            }

            groupListAdapter.submitList(tabGroupTabs)
            scrollToPosition(selectedIndex)
        }
    }

    /**
     * Notify the nested [RecyclerView] that it has been detached.
     */
    fun unbind() {
        groupListAdapter.onDetachedFromRecyclerView(binding.tabGroupList)
    }

    /**
     * Notify the nested [RecyclerView] that it has been attached. This is so our observers know when to start again.
     */
    fun rebind() {
        groupListAdapter.onAttachedToRecyclerView(binding.tabGroupList)
    }

    companion object {
        const val LAYOUT_ID = R.layout.tab_group_item
    }
}
