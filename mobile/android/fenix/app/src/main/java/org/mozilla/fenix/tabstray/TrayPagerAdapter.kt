/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.ConcatAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.InactiveTabsAdapter
import org.mozilla.fenix.tabstray.browser.InactiveTabsInteractor
import org.mozilla.fenix.tabstray.browser.TabGroupAdapter
import org.mozilla.fenix.tabstray.browser.TitleHeaderAdapter
import org.mozilla.fenix.tabstray.viewholders.AbstractPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.NormalBrowserPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.PrivateBrowserPageViewHolder
import org.mozilla.fenix.tabstray.viewholders.SyncedTabsPageViewHolder

@Suppress("LongParameterList")
class TrayPagerAdapter(
    @VisibleForTesting internal val context: Context,
    @VisibleForTesting internal val lifecycleOwner: LifecycleOwner,
    @VisibleForTesting internal val tabsTrayStore: TabsTrayStore,
    @VisibleForTesting internal val browserInteractor: BrowserTrayInteractor,
    @VisibleForTesting internal val navInteractor: NavigationInteractor,
    @VisibleForTesting internal val tabsTrayInteractor: TabsTrayInteractor,
    @VisibleForTesting internal val browserStore: BrowserStore,
    @VisibleForTesting internal val appStore: AppStore,
    @VisibleForTesting internal val inactiveTabsInteractor: InactiveTabsInteractor,
) : RecyclerView.Adapter<AbstractPageViewHolder>() {

    /**
     * ⚠️ N.B: Scrolling to the selected tab depends on the order of these adapters. If you change
     * the ordering or add/remove an adapter, please update [NormalBrowserPageViewHolder.scrollToTab] and
     * the layout manager.
     */
    private val normalAdapter by lazy {
        ConcatAdapter(
            InactiveTabsAdapter(
                lifecycleOwner = lifecycleOwner,
                tabsTrayStore = tabsTrayStore,
                inactiveTabsInteractor = inactiveTabsInteractor,
                featureName = INACTIVE_TABS_FEATURE_NAME,
            ),
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

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AbstractPageViewHolder =
        when (viewType) {
            NormalBrowserPageViewHolder.LAYOUT_ID -> {
                NormalBrowserPageViewHolder(
                    LayoutInflater.from(parent.context).inflate(viewType, parent, false),
                    lifecycleOwner,
                    tabsTrayStore,
                    browserStore,
                    appStore,
                    tabsTrayInteractor
                )
            }
            PrivateBrowserPageViewHolder.LAYOUT_ID -> {
                PrivateBrowserPageViewHolder(
                    LayoutInflater.from(parent.context).inflate(viewType, parent, false),
                    tabsTrayStore,
                    browserStore,
                    tabsTrayInteractor
                )
            }
            SyncedTabsPageViewHolder.LAYOUT_ID -> {
                SyncedTabsPageViewHolder(
                    composeView = ComposeView(parent.context).apply {
                        layoutParams = ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT
                        )
                    },
                    tabsTrayStore = tabsTrayStore,
                    navigationInteractor = navInteractor
                )
            }
            else -> throw IllegalStateException("Unknown viewType.")
        }

    /**
     * Until [TrayPagerAdapter] is replaced with a Compose implementation, [SyncedTabsPageViewHolder]
     * will need to be called with an empty bind() function since it no longer needs an adapter to render.
     * For more details: https://github.com/mozilla-mobile/fenix/issues/21318
     */
    override fun onBindViewHolder(viewHolder: AbstractPageViewHolder, position: Int) {
        when (viewHolder) {
            is NormalBrowserPageViewHolder -> viewHolder.bind(normalAdapter)
            is PrivateBrowserPageViewHolder -> viewHolder.bind(privateAdapter)
            is SyncedTabsPageViewHolder -> viewHolder.bind()
        }
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
