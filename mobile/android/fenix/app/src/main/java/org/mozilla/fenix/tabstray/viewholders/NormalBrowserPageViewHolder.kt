/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.ConcatAdapter
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.selector.selectedNormalTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tab
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.browser.InactiveTabsState
import org.mozilla.fenix.tabstray.browser.containsTabId
import org.mozilla.fenix.tabstray.browser.maxActiveTime
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.defaultBrowserLayoutColumns
import org.mozilla.fenix.tabstray.ext.inactiveTabs
import org.mozilla.fenix.tabstray.ext.titleHeaderAdapter
import org.mozilla.fenix.tabstray.ext.inactiveTabsAdapter
import org.mozilla.fenix.tabstray.ext.isNormalTabInactive
import org.mozilla.fenix.tabstray.ext.isNormalTabActiveWithSearchTerm
import org.mozilla.fenix.tabstray.ext.normalTrayTabs
import org.mozilla.fenix.tabstray.ext.observeFirstInsert
import org.mozilla.fenix.tabstray.ext.tabGroupAdapter

/**
 * View holder for the normal tabs tray list.
 */
class NormalBrowserPageViewHolder(
    containerView: View,
    private val tabsTrayStore: TabsTrayStore,
    private val browserStore: BrowserStore,
    interactor: TabsTrayInteractor,
) : AbstractBrowserPageViewHolder(containerView, tabsTrayStore, interactor), SelectionHolder<Tab> {

    /**
     * Holds the list of selected tabs.
     *
     * Implementation notes: we do this here because we only want the normal tabs list to be able
     * to select tabs.
     */
    override val selectedItems: Set<Tab>
        get() = tabsTrayStore.state.mode.selectedTabs

    override val emptyStringText: String
        get() = itemView.resources.getString(R.string.no_open_tabs_description)

    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>
    ) {
        val concatAdapter = adapter as ConcatAdapter
        val browserAdapter = concatAdapter.browserAdapter
        val tabGroupAdapter = concatAdapter.tabGroupAdapter
        val manager = setupLayoutManager(containerView.context, concatAdapter)

        browserAdapter.selectionHolder = this
        tabGroupAdapter.selectionHolder = this

        super.bind(adapter, manager)
    }

    /**
     * Add giant explanation why this is complicated.
     */
    override fun scrollToTab(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager
    ) {
        val concatAdapter = adapter as ConcatAdapter
        val headerAdapter = concatAdapter.titleHeaderAdapter
        val browserAdapter = concatAdapter.browserAdapter
        val inactiveTabAdapter = concatAdapter.inactiveTabsAdapter
        val tabGroupAdapter = concatAdapter.tabGroupAdapter

        val selectedTab = browserStore.state.selectedNormalTab ?: return

        // Update tabs into the inactive adapter.
        if (FeatureFlags.inactiveTabs && selectedTab.isNormalTabInactive(maxActiveTime)) {
            val inactiveTabsList = browserStore.state.inactiveTabs
            // We want to expand the inactive section first before we want to fire our scroll observer.
            InactiveTabsState.isExpanded = true
            inactiveTabAdapter.observeFirstInsert {
                inactiveTabsList.forEachIndexed { tabIndex, item ->
                    if (item.id == selectedTab.id) {
                        // Inactive Tabs are first + inactive header item.
                        val indexToScrollTo = tabIndex + 1
                        layoutManager.scrollToPosition(indexToScrollTo)

                        return@observeFirstInsert
                    }
                }
            }
        }

        // Updates tabs into the search term group adapter.
        if (FeatureFlags.tabGroupFeature && selectedTab.isNormalTabActiveWithSearchTerm(maxActiveTime)) {
            tabGroupAdapter.observeFirstInsert {
                // With a grouping, we need to use the list of the adapter that is already grouped
                // together for the UI, so we know the final index of the grouping to scroll to.
                //
                // N.B: Why are we using currentList here and no where else? `currentList` is an API on top of
                // `ListAdapter` which is updated when the [ListAdapter.submitList] is invoked. For our BrowserAdapter
                // as an example, the updates are coming from [TabsFeature] which internally uses the internal
                // [DiffUtil.calculateDiff] directly to submit a changed list which evades the `ListAdapter` from being
                // notified of updates, so it therefore returns an empty list.
                tabGroupAdapter.currentList.forEachIndexed { groupIndex, group ->
                    if (group.containsTabId(selectedTab.id)) {

                        // Index is based on tabs above (inactive) with our calculated index.
                        val indexToScrollTo = inactiveTabAdapter.itemCount + groupIndex
                        layoutManager.scrollToPosition(indexToScrollTo)

                        return@observeFirstInsert
                    }
                }
            }
        }

        // Updates tabs into the normal browser tabs adapter.
        browserAdapter.observeFirstInsert {
            val activeTabsList = browserStore.state.normalTrayTabs
            activeTabsList.forEachIndexed { tabIndex, trayTab ->
                if (trayTab.id == selectedTab.id) {

                    // Index is based on tabs above (inactive + groups + header) with our calculated index.
                    val indexToScrollTo = inactiveTabAdapter.itemCount +
                        tabGroupAdapter.itemCount +
                        headerAdapter.itemCount + tabIndex

                    layoutManager.scrollToPosition(indexToScrollTo)

                    return@observeFirstInsert
                }
            }
        }
    }

    private fun setupLayoutManager(
        context: Context,
        concatAdapter: ConcatAdapter
    ): GridLayoutManager {
        val headerAdapter = concatAdapter.titleHeaderAdapter
        val inactiveTabAdapter = concatAdapter.inactiveTabsAdapter
        val tabGroupAdapter = concatAdapter.tabGroupAdapter

        val numberOfColumns = containerView.context.defaultBrowserLayoutColumns
        return GridLayoutManager(context, numberOfColumns).apply {
            spanSizeLookup = object : GridLayoutManager.SpanSizeLookup() {
                override fun getSpanSize(position: Int): Int {
                    return if (position >= inactiveTabAdapter.itemCount + tabGroupAdapter.itemCount +
                        headerAdapter.itemCount
                    ) {
                        1
                    } else {
                        numberOfColumns
                    }
                }
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.normal_browser_tray_list
    }
}
