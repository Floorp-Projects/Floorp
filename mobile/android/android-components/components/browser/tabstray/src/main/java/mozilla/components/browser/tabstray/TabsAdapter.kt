/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * Function responsible for creating a `TabViewHolder` in the `TabsAdapter`.
 */
typealias ViewHolderProvider = (ViewGroup) -> TabViewHolder

/**
 * RecyclerView adapter implementation to display a list/grid of tabs.
 *
 * @param thumbnailLoader an implementation of an [ImageLoader] for loading thumbnail images in the tabs tray.
 * @param viewHolderProvider a function that creates a `TabViewHolder`.
 * @param delegate TabsTray.Observer registry to allow `TabsAdapter` to conform to `Observable<TabsTray.Observer>`.
 */
open class TabsAdapter(
    thumbnailLoader: ImageLoader? = null,
    private val viewHolderProvider: ViewHolderProvider = { parent ->
        DefaultTabViewHolder(
            LayoutInflater.from(parent.context).inflate(R.layout.mozac_browser_tabstray_item, parent, false),
            thumbnailLoader
        )
    },
    delegate: Observable<TabsTray.Observer> = ObserverRegistry()
) : RecyclerView.Adapter<TabViewHolder>(), TabsTray, Observable<TabsTray.Observer> by delegate {
    private var tabs: Tabs? = null

    var styling: TabsTrayStyling = TabsTrayStyling()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        return viewHolderProvider.invoke(parent)
    }

    override fun getItemCount() = tabs?.list?.size ?: 0

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        val tabs = tabs ?: return

        holder.bind(tabs.list[position], isTabSelected(tabs, position), styling, this)
    }

    override fun onBindViewHolder(
        holder: TabViewHolder,
        position: Int,
        payloads: List<Any>
    ) {
        tabs?.let { tabs ->
            if (tabs.list.isEmpty()) return

            if (payloads.isEmpty()) {
                onBindViewHolder(holder, position)
                return
            }

            if (payloads.contains(PAYLOAD_HIGHLIGHT_SELECTED_ITEM) && position == tabs.selectedIndex) {
                holder.updateSelectedTabIndicator(true)
            } else if (payloads.contains(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM) && position == tabs.selectedIndex) {
                holder.updateSelectedTabIndicator(false)
            }
        }
    }

    override fun updateTabs(tabs: Tabs) {
        this.tabs = tabs

        notifyObservers { onTabsUpdated() }
    }

    override fun onTabsInserted(position: Int, count: Int) = notifyItemRangeInserted(position, count)

    override fun onTabsRemoved(position: Int, count: Int) = notifyItemRangeRemoved(position, count)

    override fun onTabsMoved(fromPosition: Int, toPosition: Int) = notifyItemMoved(fromPosition, toPosition)

    override fun onTabsChanged(position: Int, count: Int) = notifyItemRangeChanged(position, count)

    override fun isTabSelected(tabs: Tabs, position: Int) = tabs.selectedIndex == position

    companion object {
        /**
         * Payload used in onBindViewHolder for a partial update of the current view.
         *
         * Signals that the currently selected tab should be highlighted. This is the default behavior.
         */
        val PAYLOAD_HIGHLIGHT_SELECTED_ITEM: Int = R.id.payload_highlight_selected_item

        /**
         * Payload used in onBindViewHolder for a partial update of the current view.
         *
         * Signals that the currently selected tab should NOT be highlighted. No tabs would appear as highlighted.
         */
        val PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM: Int = R.id.payload_dont_highlight_selected_item
    }
}
