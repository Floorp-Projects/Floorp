/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.SelectableTabViewHolder
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.TabTrayGridItemBinding
import org.mozilla.fenix.databinding.TabTrayItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.topsites.dpToPx
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.browser.compose.ComposeListViewHolder
import org.mozilla.fenix.tabstray.ext.MIN_COLUMN_WIDTH_DP

/**
 * The [ListAdapter] for displaying the list of tabs that have the same search term.
 *
 * @param context [Context] used for various platform interactions or accessing [Components]
 * @param interactor [BrowserTrayInteractor] handling tabs interactions in a tab tray.
 * @param store [TabsTrayStore] containing the complete state of tabs tray and methods to update that.
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 * @param viewLifecycleOwner [LifecycleOwner] life cycle owner for the view.
 */
class TabGroupListAdapter(
    private val context: Context,
    private val interactor: BrowserTrayInteractor,
    private val store: TabsTrayStore,
    private val selectionHolder: SelectionHolder<TabSessionState>?,
    private val featureName: String,
    private val viewLifecycleOwner: LifecycleOwner
) : ListAdapter<TabSessionState, SelectableTabViewHolder>(DiffCallback) {

    private val selectedItemAdapterBinding = SelectedItemAdapterBinding(store, this)
    private val imageLoader = ThumbnailLoader(context.components.core.thumbnailStorage)
    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): SelectableTabViewHolder {
        return when {
            context.components.settings.gridTabView -> {
                val view = LayoutInflater.from(parent.context).inflate(R.layout.tab_tray_grid_item, parent, false)
                view.layoutParams.width = view.dpToPx(MIN_COLUMN_WIDTH_DP.toFloat())
                BrowserTabViewHolder.GridViewHolder(imageLoader, interactor, store, selectionHolder, view, featureName)
            }
            else -> {
                if (FeatureFlags.composeTabsTray) {
                    ComposeListViewHolder(
                        interactor = interactor,
                        tabsTrayStore = store,
                        selectionHolder = selectionHolder,
                        composeItemView = ComposeView(parent.context),
                        featureName = featureName,
                        viewLifecycleOwner = viewLifecycleOwner
                    )
                } else {
                    val view = LayoutInflater.from(parent.context)
                        .inflate(R.layout.tab_tray_item, parent, false)
                    BrowserTabViewHolder.ListViewHolder(
                        imageLoader,
                        interactor,
                        store,
                        selectionHolder,
                        view,
                        featureName
                    )
                }
            }
        }
    }

    override fun onBindViewHolder(holder: SelectableTabViewHolder, position: Int) {
        val tab = getItem(position)
        val selectedTabId = context.components.core.store.state.selectedTabId
        holder.bind(tab, tab.id == selectedTabId, TabsTrayStyling(), interactor)
        holder.tab?.let { holderTab ->
            when {
                context.components.settings.gridTabView -> {
                    val gridBinding = TabTrayGridItemBinding.bind(holder.itemView)
                    gridBinding.mozacBrowserTabstrayClose.setOnClickListener {
                        interactor.close(holderTab, featureName)
                    }
                }
                else -> {
                    val listBinding = TabTrayItemBinding.bind(holder.itemView)
                    listBinding.mozacBrowserTabstrayClose.setOnClickListener {
                        interactor.close(holderTab, featureName)
                    }
                }
            }
        }
    }

    /**
     * Over-ridden [onBindViewHolder] that uses the payloads to notify the selected tab how to
     * display itself.
     *
     * N.B: this is a modified version of [BrowserTabsAdapter.onBindViewHolder].
     */
    override fun onBindViewHolder(holder: SelectableTabViewHolder, position: Int, payloads: List<Any>) {
        val tabs = currentList
        val selectedTabId = context.components.core.store.state.selectedTabId
        val selectedIndex = tabs.indexOfFirst { it.id == selectedTabId }

        if (tabs.isEmpty()) return

        if (payloads.isEmpty()) {
            onBindViewHolder(holder, position)
            return
        }

        if (position == selectedIndex) {
            if (payloads.contains(PAYLOAD_HIGHLIGHT_SELECTED_ITEM)) {
                holder.updateSelectedTabIndicator(true)
            } else if (payloads.contains(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM)) {
                holder.updateSelectedTabIndicator(false)
            }
        }

        selectionHolder?.let {
            var selectedMaskView: View? = null
            when (getItemViewType(position)) {
                BrowserTabsAdapter.ViewType.GRID.layoutRes -> {
                    val gridBinding = TabTrayGridItemBinding.bind(holder.itemView)
                    selectedMaskView = gridBinding.checkboxInclude.selectedMask
                }
                BrowserTabsAdapter.ViewType.LIST.layoutRes -> {
                    val listBinding = TabTrayItemBinding.bind(holder.itemView)
                    selectedMaskView = listBinding.checkboxInclude.selectedMask
                }
            }
            holder.showTabIsMultiSelectEnabled(
                selectedMaskView,
                it.selectedItems.contains(holder.tab)
            )
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when {
            context.components.settings.gridTabView -> {
                BrowserTabsAdapter.ViewType.GRID.layoutRes
            }
            else -> {
                if (FeatureFlags.composeTabsTray) {
                    BrowserTabsAdapter.ViewType.COMPOSE_LIST.layoutRes
                } else {
                    BrowserTabsAdapter.ViewType.LIST.layoutRes
                }
            }
        }
    }

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        selectedItemAdapterBinding.start()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        selectedItemAdapterBinding.stop()
    }

    private object DiffCallback : DiffUtil.ItemCallback<TabSessionState>() {
        override fun areItemsTheSame(oldItem: TabSessionState, newItem: TabSessionState) = oldItem.id == newItem.id
        override fun areContentsTheSame(oldItem: TabSessionState, newItem: TabSessionState) = oldItem == newItem
    }
}
