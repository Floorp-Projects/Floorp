/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.content.Context
import android.view.View
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.ConcatAdapter
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.selectedNormalTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.maxActiveTime
import org.mozilla.fenix.ext.potentialInactiveTabs
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.defaultBrowserLayoutColumns
import org.mozilla.fenix.tabstray.ext.getNormalTrayTabs
import org.mozilla.fenix.tabstray.ext.inactiveTabsAdapter
import org.mozilla.fenix.tabstray.ext.isNormalTabInactive
import org.mozilla.fenix.tabstray.ext.observeFirstInsert

/**
 * View holder for the normal tabs tray list.
 */
class NormalBrowserPageViewHolder(
    containerView: View,
    private val lifecycleOwner: LifecycleOwner,
    private val tabsTrayStore: TabsTrayStore,
    private val browserStore: BrowserStore,
    private val appStore: AppStore,
    interactor: TabsTrayInteractor,
) : AbstractBrowserPageViewHolder(containerView, tabsTrayStore, interactor), SelectionHolder<TabSessionState> {

    private var inactiveTabsSize = 0

    /**
     * Holds the list of selected tabs.
     *
     * Implementation notes: we do this here because we only want the normal tabs list to be able
     * to select tabs.
     */
    override val selectedItems: Set<TabSessionState>
        get() = tabsTrayStore.state.mode.selectedTabs

    override val emptyStringText: String
        get() = itemView.resources.getString(R.string.no_open_tabs_description)

    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
    ) {
        val concatAdapter = adapter as ConcatAdapter
        val browserAdapter = concatAdapter.browserAdapter
        val manager = setupLayoutManager(containerView.context, concatAdapter)
        browserAdapter.selectionHolder = this

        observeTabsTrayInactiveTabsState(adapter)

        super.bind(adapter, manager)
    }

    /**
     * Add giant explanation why this is complicated.
     */
    override fun scrollToTab(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager,
    ) {
        val concatAdapter = adapter as ConcatAdapter
        val browserAdapter = concatAdapter.browserAdapter
        val inactiveTabAdapter = concatAdapter.inactiveTabsAdapter
        val inactiveTabsAreEnabled = containerView.context.settings().inactiveTabsAreEnabled

        val selectedTab = browserStore.state.selectedNormalTab ?: return

        // Update tabs into the inactive adapter.
        if (inactiveTabsAreEnabled && selectedTab.isNormalTabInactive(maxActiveTime)) {
            val inactiveTabsList = browserStore.state.potentialInactiveTabs
            // We want to expand the inactive section first before we want to fire our scroll observer.

            appStore.dispatch(AppAction.UpdateInactiveExpanded(true))

            inactiveTabAdapter.observeFirstInsert {
                inactiveTabsList.forEach { item ->
                    if (item.id == selectedTab.id) {
                        containerView.post { layoutManager.scrollToPosition(0) }

                        return@observeFirstInsert
                    }
                }
            }
        } else {
            // Updates tabs into the normal browser tabs adapter.
            browserAdapter.observeFirstInsert {
                val activeTabsList = browserStore.state.getNormalTrayTabs(inactiveTabsAreEnabled)
                activeTabsList.forEachIndexed { tabIndex, trayTab ->
                    if (trayTab.id == selectedTab.id) {
                        // Index is based on tabs above (inactive) with our calculated index.
                        val indexToScrollTo = inactiveTabAdapter.itemCount + tabIndex

                        containerView.post { layoutManager.scrollToPosition(indexToScrollTo) }

                        return@observeFirstInsert
                    }
                }
            }
        }
    }

    // Temporary hack until https://github.com/mozilla-mobile/fenix/issues/21901 where the
    // logic that shows/hides the "Your open tabs will be shown here." message will no longer be derived
    // from adapters, view holders, and item counts.
    override fun showTrayList(adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>): Boolean {
        return inactiveTabsSize > 0 || adapter.itemCount > 1 // InactiveTabsAdapter will always return 1
    }

    private fun observeTabsTrayInactiveTabsState(adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>) {
        tabsTrayStore.flowScoped(lifecycleOwner) { flow ->
            flow.map { state -> state.inactiveTabs }
                .distinctUntilChanged()
                .collect { inactiveTabs ->
                    inactiveTabsSize = inactiveTabs.size
                    updateTrayVisibility(showTrayList(adapter))
                }
        }
    }

    private fun setupLayoutManager(
        context: Context,
        concatAdapter: ConcatAdapter,
    ): GridLayoutManager {
        val inactiveTabAdapter = concatAdapter.inactiveTabsAdapter
        val numberOfColumns = containerView.context.defaultBrowserLayoutColumns
        return GridLayoutManager(context, numberOfColumns).apply {
            spanSizeLookup = object : GridLayoutManager.SpanSizeLookup() {
                override fun getSpanSize(position: Int): Int {
                    return if (position >= inactiveTabAdapter.itemCount) {
                        1
                    } else {
                        numberOfColumns
                    }
                }
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.normal_browser_tray_list
    }
}
