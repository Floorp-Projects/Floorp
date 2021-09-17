/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.View
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.TabGroupItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.TrayPagerAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.TabGroupAdapter
import org.mozilla.fenix.tabstray.browser.TabGroupListAdapter

/**
 * A RecyclerView ViewHolder implementation for tab group items.
 *
 * @param itemView [View] that displays a "tab".
 * @param orientation [Int] orientation of the items.  Horizontal for grid layout, vertical for list layout
 * @param interactor the [BrowserTrayInteractor] for tab interactions.
 * @param store the [TabsTrayStore] instance.
 * @param selectionHolder the store that holds the currently selected tabs.
 */
class TabGroupViewHolder(
    itemView: View,
    val orientation: Int,
    val interactor: BrowserTrayInteractor,
    val store: TabsTrayStore,
    val selectionHolder: SelectionHolder<Tab>? = null
) : RecyclerView.ViewHolder(itemView) {
    private val binding = TabGroupItemBinding.bind(itemView)

    lateinit var groupListAdapter: TabGroupListAdapter

    fun bind(
        group: TabGroupAdapter.Group,
        observable: Observable<TabsTray.Observer>
    ) {
        val selectedTabId = itemView.context.components.core.store.state.selectedTabId
        val selectedIndex = group.tabs.indexOfFirst { it.id == selectedTabId }

        binding.tabGroupTitle.text = group.title
        binding.tabGroupList.apply {
            layoutManager = LinearLayoutManager(itemView.context, orientation, false)
            groupListAdapter = TabGroupListAdapter(
                context = itemView.context,
                interactor = interactor,
                store = store,
                delegate = observable,
                selectionHolder = selectionHolder,
                featureName = TrayPagerAdapter.TAB_GROUP_FEATURE_NAME
            )

            adapter = groupListAdapter

            groupListAdapter.submitList(group.tabs)
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
