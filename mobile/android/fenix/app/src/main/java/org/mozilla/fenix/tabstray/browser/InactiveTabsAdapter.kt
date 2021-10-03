/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.concept.tabstray.Tab as TabsTrayTab
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.FooterHolder
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.HeaderHolder
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.TabViewHolder
import org.mozilla.fenix.tabstray.ext.autoCloseInterval
import mozilla.components.support.base.observer.Observable as ComponentObservable

/**
 * A convenience alias for readability.
 */
private typealias Adapter = ListAdapter<InactiveTabsAdapter.Item, InactiveTabViewHolder>

/**
 * A convenience alias for readability.
 */
private typealias Observable = ComponentObservable<TabsTray.Observer>

/**
 * The [ListAdapter] for displaying the list of inactive tabs.
 *
 * @param context [Context] used for various platform interactions or accessing [Components]
 * @param browserTrayInteractor [BrowserTrayInteractor] handling tabs interactions in a tab tray.
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 * @param delegate [Observable]<[TabsTray.Observer]> for observing tabs tray changes. Defaults to [ObserverRegistry].
 */
class InactiveTabsAdapter(
    private val context: Context,
    private val browserTrayInteractor: BrowserTrayInteractor,
    private val tabsTrayInteractor: TabsTrayInteractor,
    private val featureName: String,
    delegate: Observable = ObserverRegistry()
) : Adapter(DiffCallback), TabsTray, Observable by delegate {

    internal lateinit var inactiveTabsInteractor: InactiveTabsInteractor

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): InactiveTabViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(viewType, parent, false)

        return when (viewType) {
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
            is FooterHolder -> {
                val item = getItem(position) as Item.Footer
                holder.bind(item.interval)
            }
            is HeaderHolder -> {
                // do nothing.
            }
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when (position) {
            0 -> HeaderHolder.LAYOUT_ID
            itemCount - 1 -> FooterHolder.LAYOUT_ID
            else -> TabViewHolder.LAYOUT_ID
        }
    }

    override fun updateTabs(tabs: Tabs) {
        // Early return with an empty list to remove the header/footer items.
        if (tabs.list.isEmpty()) {
            submitList(emptyList())
            return
        }

        // If we have items, but we should be in a collapsed state.
        if (!InactiveTabsState.isExpanded) {
            submitList(listOf(Item.Header))
            return
        }

        val items = tabs.list.map { Item.Tab(it) }
        val footer = Item.Footer(context.autoCloseInterval)

        submitList(listOf(Item.Header) + items + listOf(footer))
    }

    override fun isTabSelected(tabs: Tabs, position: Int): Boolean = false
    override fun onTabsChanged(position: Int, count: Int) = Unit
    override fun onTabsInserted(position: Int, count: Int) = Unit
    override fun onTabsMoved(fromPosition: Int, toPosition: Int) = Unit
    override fun onTabsRemoved(position: Int, count: Int) = Unit

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
        data class Tab(val tab: TabsTrayTab) : Item()

        /**
         * A footer for the inactive tab section. This may be seen only
         * when at least one inactive tab is present.
         */
        data class Footer(val interval: AutoCloseInterval) : Item()
    }
}
