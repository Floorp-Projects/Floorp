/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.tab_tray_item.view.*
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * A [RecyclerView.Adapter] for browser tabs.
 */
class BrowserTabsAdapter(
    private val context: Context,
    private val interactor: BrowserTrayInteractor,
    private val store: TabsTrayStore,
    delegate: Observable<TabsTray.Observer> = ObserverRegistry()
) : TabsAdapter<AbstractBrowserTabViewHolder>(delegate) {

    /**
     * The layout types for the tabs.
     */
    enum class ViewType(val layoutRes: Int) {
        LIST(R.layout.tab_tray_item),
        GRID(R.layout.tab_tray_grid_item)
    }

    /**
     * Tracks the selected tabs in multi-select mode.
     */
    var selectionHolder: SelectionHolder<Tab>? = null

    private val selectedItemAdapterBinding = SelectedItemAdapterBinding(store, this)
    private val imageLoader = ThumbnailLoader(context.components.core.thumbnailStorage)

    override fun getItemViewType(position: Int): Int {
        return if (context.components.settings.gridTabView) {
            ViewType.GRID.layoutRes
        } else {
            ViewType.LIST.layoutRes
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AbstractBrowserTabViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            ViewType.GRID.layoutRes ->
                BrowserTabGridViewHolder(imageLoader, interactor, store, selectionHolder, view)
            else ->
                BrowserTabListViewHolder(imageLoader, interactor, store, selectionHolder, view)
        }
    }

    override fun onBindViewHolder(holder: AbstractBrowserTabViewHolder, position: Int) {
        super.onBindViewHolder(holder, position)

        holder.tab?.let { tab ->
            holder.itemView.mozac_browser_tabstray_close.setOnClickListener {
                interactor.close(tab)
            }

            selectionHolder?.let {
                holder.showTabIsMultiSelectEnabled(it.selectedItems.contains(tab))
            }
        }
    }

    /**
     * Over-ridden [onBindViewHolder] that uses the payloads to notify the selected tab how to
     * display itself.
     */
    override fun onBindViewHolder(holder: AbstractBrowserTabViewHolder, position: Int, payloads: List<Any>) {
        val tabs = tabs ?: return

        if (tabs.list.isEmpty()) return

        if (payloads.isEmpty()) {
            onBindViewHolder(holder, position)
            return
        }

        if (position == tabs.selectedIndex) {
            if (payloads.contains(PAYLOAD_HIGHLIGHT_SELECTED_ITEM)) {
                holder.updateSelectedTabIndicator(true)
            } else if (payloads.contains(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM)) {
                holder.updateSelectedTabIndicator(false)
            }
        }

        selectionHolder?.let {
            holder.showTabIsMultiSelectEnabled(it.selectedItems.contains(holder.tab))
        }
    }

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        selectedItemAdapterBinding.start()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        selectedItemAdapterBinding.stop()
    }
}
