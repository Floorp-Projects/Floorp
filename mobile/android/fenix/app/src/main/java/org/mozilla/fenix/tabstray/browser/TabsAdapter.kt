/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.annotation.CallSuper
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * RecyclerView adapter implementation to display a list/grid of tabs.
 *
 * The previous tabs adapter was very restrictive and required Fenix to jump through
 * may hoops to access and update certain methods. An abstract adapter is easier to manage
 * for Android UI APIs.
 *
 * TODO Let's upstream this to AC with tests.
 *
 * @param delegate TabsTray.Observer registry to allow `TabsAdapter` to conform to `Observable<TabsTray.Observer>`.
 */
abstract class TabsAdapter<T : TabViewHolder>(
    delegate: Observable<TabsTray.Observer> = ObserverRegistry()
) : ListAdapter<Tab, T>(DiffCallback), TabsTray, Observable<TabsTray.Observer> by delegate {

    protected var tabs: Tabs? = null
    protected var styling: TabsTrayStyling = TabsTrayStyling()

    @CallSuper
    override fun updateTabs(tabs: Tabs) {
        this.tabs = tabs

        submitList(tabs.list)

        notifyObservers { onTabsUpdated() }
    }

    @CallSuper
    override fun onBindViewHolder(holder: T, position: Int) {
        val tabs = tabs ?: return

        holder.bind(tabs.list[position], isTabSelected(tabs, position), styling, this)
    }

    final override fun isTabSelected(tabs: Tabs, position: Int): Boolean =
        tabs.selectedIndex == position

    private object DiffCallback : DiffUtil.ItemCallback<Tab>() {
        override fun areItemsTheSame(oldItem: Tab, newItem: Tab): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: Tab, newItem: Tab): Boolean {
            return oldItem == newItem
        }
    }
}
