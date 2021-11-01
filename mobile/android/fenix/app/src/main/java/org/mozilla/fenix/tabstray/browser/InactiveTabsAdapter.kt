/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.AutoCloseDialogHolder
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.FooterHolder
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.HeaderHolder
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.TabViewHolder
import org.mozilla.fenix.utils.Settings

/**
 * A convenience alias for readability.
 */
private typealias Adapter = ListAdapter<InactiveTabsAdapter.Item, InactiveTabViewHolder>

/**
 * The [ListAdapter] for displaying the list of inactive tabs.
 *
 * @param context [Context] used for various platform interactions or accessing [Components]
 * @param browserTrayInteractor [BrowserTrayInteractor] handling tabs interactions in a tab tray.
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 */
class InactiveTabsAdapter(
    private val context: Context,
    private val browserTrayInteractor: BrowserTrayInteractor,
    private val tabsTrayInteractor: TabsTrayInteractor,
    override val featureName: String,
    private val settings: Settings,
) : Adapter(DiffCallback), TabsTray, FeatureNameHolder {

    internal lateinit var inactiveTabsInteractor: InactiveTabsInteractor
    internal var inActiveTabsCount: Int = 0

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): InactiveTabViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(viewType, parent, false)

        return when (viewType) {
            AutoCloseDialogHolder.LAYOUT_ID -> AutoCloseDialogHolder(view, inactiveTabsInteractor)
            HeaderHolder.LAYOUT_ID -> HeaderHolder(view, inactiveTabsInteractor, tabsTrayInteractor)
            TabViewHolder.LAYOUT_ID -> TabViewHolder(view, browserTrayInteractor, featureName)
            FooterHolder.LAYOUT_ID -> FooterHolder(view)
            else -> throw IllegalStateException("Unknown viewType: $viewType")
        }
    }

    override fun onBindViewHolder(holder: InactiveTabViewHolder, position: Int) {
        when (holder) {
            is TabViewHolder -> {
                val item = getItem(position) as Item.Tab
                holder.bind(item.tab)
            }

            is FooterHolder, is HeaderHolder, is AutoCloseDialogHolder -> {
                // do nothing.
            }
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when (position) {
            0 -> HeaderHolder.LAYOUT_ID
            1 -> if (settings.shouldShowInactiveTabsAutoCloseDialog(inActiveTabsCount)) {
                AutoCloseDialogHolder.LAYOUT_ID
            } else {
                TabViewHolder.LAYOUT_ID
            }
            itemCount - 1 -> FooterHolder.LAYOUT_ID
            else -> TabViewHolder.LAYOUT_ID
        }
    }

    override fun updateTabs(tabs: List<TabSessionState>, selectedTabId: String?) {
        inActiveTabsCount = tabs.size

        // Early return with an empty list to remove the header/footer items.
        if (tabs.isEmpty()) {
            submitList(emptyList())
            return
        }

        // If we have items, but we should be in a collapsed state.
        if (!context.components.appStore.state.inactiveTabsExpanded) {
            submitList(listOf(Item.Header))
            return
        }

        val items = tabs.map { Item.Tab(it) }
        val footer = Item.Footer
        val headerItems = if (settings.shouldShowInactiveTabsAutoCloseDialog(items.size)) {
            listOf(Item.Header, Item.AutoCloseMessage)
        } else {
            listOf(Item.Header)
        }
        submitList(headerItems + items + listOf(footer))
    }

    private object DiffCallback : DiffUtil.ItemCallback<Item>() {
        override fun areItemsTheSame(oldItem: Item, newItem: Item): Boolean {
            return if (oldItem is Item.Tab && newItem is Item.Tab) {
                oldItem.tab.id == newItem.tab.id
            } else {
                oldItem == newItem
            }
        }

        override fun areContentsTheSame(oldItem: Item, newItem: Item): Boolean {
            return oldItem == newItem
        }
    }

    /**
     * The types of different data we can put into the [InactiveTabsAdapter].
     */
    sealed class Item {

        /**
         * A title header for the inactive tab section. This may be seen only
         * when at least one inactive tab is present.
         */
        object Header : Item()

        /**
         * A tab that is now considered inactive.
         */
        data class Tab(val tab: TabSessionState) : Item()

        /**
         * A dialog for when the inactive tabs section reach 20 tabs.
         */
        object AutoCloseMessage : Item()

        /**
         * A footer for the inactive tab section. This may be seen only
         * when at least one inactive tab is present.
         */
        object Footer : Item()
    }
}
