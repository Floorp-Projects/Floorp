/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.ConcatAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.sync.SyncedTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.TitleHeaderAdapter
import org.mozilla.fenix.tabstray.browser.InactiveTabsAdapter
import org.mozilla.fenix.tabstray.browser.TabGroupAdapter
import org.mozilla.fenix.tabstray.syncedtabs.TabClickDelegate
import org.mozilla.fenix.tabstray.viewholders.AbstractPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.NormalBrowserPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.PrivateBrowserPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.SyncedTabsPageViewHolder

@Suppress("LongParameterList")
class TrayPagerAdapter(
    @VisibleForTesting internal val context: Context,
    @VisibleForTesting internal val tabsTrayStore: TabsTrayStore,
    @VisibleForTesting internal val browserInteractor: BrowserTrayInteractor,
    @VisibleForTesting internal val navInteractor: NavigationInteractor,
    @VisibleForTesting internal val interactor: TabsTrayInteractor,
    @VisibleForTesting internal val browserStore: BrowserStore,
    @VisibleForTesting internal val appStore: AppStore
) : RecyclerView.Adapter<AbstractPageViewHolder>() {

    /**
     * ⚠️ N.B: Scrolling to the selected tab depends on the order of these adapters. If you change
     * the ordering or add/remove an adapter, please update [NormalBrowserPageViewHolder.scrollToTab] and
     * the layout manager.
     */
    private val normalAdapter by lazy {
        ConcatAdapter(
            InactiveTabsAdapter(context, browserInteractor, interactor, INACTIVE_TABS_FEATURE_NAME, context.settings()),
            TabGroupAdapter(context, browserInteractor, tabsTrayStore, TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, browserInteractor, tabsTrayStore, TABS_TRAY_FEATURE_NAME)
        )
    }
    private val privateAdapter by lazy {
        BrowserTabsAdapter(
            context,
            browserInteractor,
            tabsTrayStore,
            TABS_TRAY_FEATURE_NAME
        )
    }
    private val syncedTabsAdapter by lazy {
        SyncedTabsAdapter(TabClickDelegate(navInteractor))
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AbstractPageViewHolder {
        val itemView = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            NormalBrowserPageViewHolder.LAYOUT_ID -> {
                NormalBrowserPageViewHolder(
                    itemView,
                    tabsTrayStore,
                    browserStore,
                    appStore,
                    interactor
                )
            }
            PrivateBrowserPageViewHolder.LAYOUT_ID -> {
                PrivateBrowserPageViewHolder(
                    itemView,
                    tabsTrayStore,
                    browserStore,
                    interactor
                )
            }
            SyncedTabsPageViewHolder.LAYOUT_ID -> {
                SyncedTabsPageViewHolder(
                    itemView,
                    tabsTrayStore
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
        viewHolder.bind(adapter)
    }

    override fun getItemViewType(position: Int): Int {
        return when (position) {
            POSITION_NORMAL_TABS -> NormalBrowserPageViewHolder.LAYOUT_ID
            POSITION_PRIVATE_TABS -> PrivateBrowserPageViewHolder.LAYOUT_ID
            POSITION_SYNCED_TABS -> SyncedTabsPageViewHolder.LAYOUT_ID
            else -> throw IllegalStateException("Unknown position.")
        }
    }

    override fun onViewAttachedToWindow(holder: AbstractPageViewHolder) {
        holder.attachedToWindow()
    }

    override fun onViewDetachedFromWindow(holder: AbstractPageViewHolder) {
        holder.detachedFromWindow()
    }

    override fun getItemCount(): Int = TRAY_TABS_COUNT

    companion object {
        const val TRAY_TABS_COUNT = 3

        // Telemetry keys for identifying from which app features the a was opened / closed.
        const val TABS_TRAY_FEATURE_NAME = "Tabs tray"
        const val TAB_GROUP_FEATURE_NAME = "Tab group"
        const val INACTIVE_TABS_FEATURE_NAME = "Inactive tabs"

        val POSITION_NORMAL_TABS = Page.NormalTabs.ordinal
        val POSITION_PRIVATE_TABS = Page.PrivateTabs.ordinal
        val POSITION_SYNCED_TABS = Page.SyncedTabs.ordinal
    }
}
