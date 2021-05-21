/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.sync.SyncedTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.syncedtabs.TabClickDelegate
import org.mozilla.fenix.tabstray.viewholders.AbstractPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.NormalBrowserPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.PrivateBrowserPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.SyncedTabsPageViewHolder

class TrayPagerAdapter(
    @VisibleForTesting internal val context: Context,
    @VisibleForTesting internal val store: TabsTrayStore,
    @VisibleForTesting internal val browserInteractor: BrowserTrayInteractor,
    @VisibleForTesting internal val navInteractor: NavigationInteractor,
    @VisibleForTesting internal val interactor: TabsTrayInteractor,
    @VisibleForTesting internal val browserStore: BrowserStore
) : RecyclerView.Adapter<AbstractPageViewHolder>() {

    private val normalAdapter by lazy { BrowserTabsAdapter(context, browserInteractor, store) }
    private val privateAdapter by lazy { BrowserTabsAdapter(context, browserInteractor, store) }
    private val syncedTabsAdapter by lazy { SyncedTabsAdapter(TabClickDelegate(navInteractor)) }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AbstractPageViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        val selectedTab = browserStore.state.selectedTab

        return when (viewType) {
            NormalBrowserPageViewHolder.LAYOUT_ID -> {
                NormalBrowserPageViewHolder(
                    itemView,
                    store,
                    interactor,
                    browserStore.state.normalTabs.indexOf(selectedTab)
                )
            }
            PrivateBrowserPageViewHolder.LAYOUT_ID -> {
                PrivateBrowserPageViewHolder(
                    itemView,
                    store,
                    interactor,
                    browserStore.state.privateTabs.indexOf(selectedTab)
                )
            }
            SyncedTabsPageViewHolder.LAYOUT_ID -> {
                SyncedTabsPageViewHolder(
                    itemView,
                    store
                )
            }
            else -> throw IllegalStateException("Unknown viewType.")
        }
    }

    override fun onBindViewHolder(viewHolder: AbstractPageViewHolder, position: Int) {
        val adapter = when (position) {
            POSITION_NORMAL_TABS -> normalAdapter
            POSITION_PRIVATE_TABS -> privateAdapter
            POSITION_SYNCED_TABS -> syncedTabsAdapter
            else -> throw IllegalStateException("View type does not exist.")
        }
        viewHolder.bind(adapter, browserInteractor.getLayoutManagerForPosition(context, position))
    }

    override fun getItemViewType(position: Int): Int {
        return when (position) {
            POSITION_NORMAL_TABS -> NormalBrowserPageViewHolder.LAYOUT_ID
            POSITION_PRIVATE_TABS -> PrivateBrowserPageViewHolder.LAYOUT_ID
            POSITION_SYNCED_TABS -> SyncedTabsPageViewHolder.LAYOUT_ID
            else -> throw IllegalStateException("Unknown position.")
        }
    }

    override fun getItemCount(): Int = TRAY_TABS_COUNT

    companion object {
        const val TRAY_TABS_COUNT = 3

        val POSITION_NORMAL_TABS = Page.NormalTabs.ordinal
        val POSITION_PRIVATE_TABS = Page.PrivateTabs.ordinal
        val POSITION_SYNCED_TABS = Page.SyncedTabs.ordinal
    }
}
