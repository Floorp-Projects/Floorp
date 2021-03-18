/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.annotation.CallSuper
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

// The previous tabs adapter was very restrictive and required Fenix to jump through
// may hoops to access and update certain methods. An abstract adapter is easier to manage
// for Android UI APIs.
//
// TODO Let's upstream this to AC with tests.
abstract class TabsAdapter(
    delegate: Observable<TabsTray.Observer> = ObserverRegistry()
) : RecyclerView.Adapter<TabViewHolder>(), TabsTray, Observable<TabsTray.Observer> by delegate {
    private var tabs: Tabs? = null

    var styling: TabsTrayStyling = TabsTrayStyling()

    override fun getItemCount(): Int = tabs?.list?.size ?: 0

    @CallSuper
    override fun updateTabs(tabs: Tabs) {
        this.tabs = tabs

        notifyObservers { onTabsUpdated() }
    }

    @CallSuper
    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        val tabs = tabs ?: return

        holder.bind(tabs.list[position], isTabSelected(tabs, position), styling, this)
    }

    final override fun isTabSelected(tabs: Tabs, position: Int): Boolean =
        tabs.selectedIndex == position

    final override fun onTabsChanged(position: Int, count: Int) =
        notifyItemRangeChanged(position, count)

    final override fun onTabsInserted(position: Int, count: Int) =
        notifyItemRangeInserted(position, count)

    final override fun onTabsMoved(fromPosition: Int, toPosition: Int) =
        notifyItemMoved(fromPosition, toPosition)

    final override fun onTabsRemoved(position: Int, count: Int) =
        notifyItemRangeRemoved(position, count)
}
