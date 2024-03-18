/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.base.images.ImageLoader

/**
 * Function responsible for creating a `TabViewHolder` in the `TabsAdapter`.
 */
typealias ViewHolderProvider = (ViewGroup) -> TabViewHolder

/**
 * RecyclerView adapter implementation to display a list of tabs.
 *
 * @param thumbnailLoader an implementation of an [ImageLoader] for loading thumbnail images in the tabs tray.
 * @param viewHolderProvider a function that creates a [TabViewHolder].
 * @param styling the default styling for the [TabsTrayStyling].
 * @param delegate a delegate to handle interactions in the tabs tray.
 */
open class TabsAdapter(
    thumbnailLoader: ImageLoader? = null,
    private val viewHolderProvider: ViewHolderProvider = { parent ->
        DefaultTabViewHolder(
            LayoutInflater.from(parent.context).inflate(R.layout.mozac_browser_tabstray_item, parent, false),
            thumbnailLoader,
        )
    },
    private val styling: TabsTrayStyling = TabsTrayStyling(),
    private val delegate: TabsTray.Delegate,
) : ListAdapter<TabSessionState, TabViewHolder>(DiffCallback), TabsTray {

    private var selectedTabId: String? = null

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        return viewHolderProvider.invoke(parent)
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        val tab = getItem(position)

        holder.bind(tab, tab.id == selectedTabId, styling, delegate)
    }

    override fun onBindViewHolder(
        holder: TabViewHolder,
        position: Int,
        payloads: List<Any>,
    ) {
        val tabs = currentList
        if (tabs.isEmpty()) return

        if (payloads.isEmpty()) {
            onBindViewHolder(holder, position)
            return
        }

        val tab = getItem(position)

        if (payloads.contains(PAYLOAD_HIGHLIGHT_SELECTED_ITEM) && tab.id == selectedTabId) {
            holder.updateSelectedTabIndicator(true)
        } else if (payloads.contains(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM) && tab.id == selectedTabId) {
            holder.updateSelectedTabIndicator(false)
        }
    }

    override fun updateTabs(tabs: List<TabSessionState>, tabPartition: TabPartition?, selectedTabId: String?) {
        this.selectedTabId = selectedTabId

        submitList(tabs)
    }

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

    private object DiffCallback : DiffUtil.ItemCallback<TabSessionState>() {
        override fun areItemsTheSame(oldItem: TabSessionState, newItem: TabSessionState): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: TabSessionState, newItem: TabSessionState): Boolean {
            return oldItem == newItem
        }
    }
}
