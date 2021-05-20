/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.tabstray.Tab
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.selection.SelectionInteractor
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.tabstray.TabsTrayController
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TrayPagerAdapter
import org.mozilla.fenix.tabstray.ext.numberOfGridColumns
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * For interacting with UI that is specifically for [BrowserTrayList] and other browser
 * tab tray views.
 */
interface BrowserTrayInteractor : SelectionInteractor<Tab>, UserInteractionHandler {

    /**
     * Close the tab.
     */
    fun close(tab: Tab)

    /**
     * Returns the appropriate [RecyclerView.LayoutManager] to be used at [position].
     */
    fun getLayoutManagerForPosition(context: Context, position: Int): RecyclerView.LayoutManager

    /**
     * TabTray's Floating Action Button clicked.
     */
    fun onFabClicked(isPrivate: Boolean)
}

/**
 * A default implementation of [BrowserTrayInteractor].
 */
class DefaultBrowserTrayInteractor(
    private val store: TabsTrayStore,
    private val trayInteractor: TabsTrayInteractor,
    private val controller: TabsTrayController,
    private val selectTab: TabsUseCases.SelectTabUseCase,
    private val settings: Settings,
    private val metrics: MetricController
) : BrowserTrayInteractor {

    private val selectTabWrapper by lazy {
        SelectTabUseCaseWrapper(metrics, selectTab) {
            trayInteractor.onBrowserTabSelected()
        }
    }

    private val removeTabWrapper by lazy {
        RemoveTabUseCaseWrapper(metrics) {
            // Handle removal from the interactor where we can also handle "undo" visuals.
            trayInteractor.onDeleteTab(it)
        }
    }

    /**
     * See [SelectionInteractor.open]
     */
    override fun open(item: Tab) {
        selectTabWrapper.invoke(item.id)
    }

    /**
     * See [BrowserTrayInteractor.close].
     */
    override fun close(tab: Tab) {
        removeTabWrapper.invoke(tab.id)
    }

    /**
     * See [SelectionInteractor.select]
     */
    override fun select(item: Tab) {
        store.dispatch(TabsTrayAction.AddSelectTab(item))
    }

    /**
     * See [SelectionInteractor.deselect]
     */
    override fun deselect(item: Tab) {
        store.dispatch(TabsTrayAction.RemoveSelectTab(item))
    }

    /**
     * See [UserInteractionHandler.onBackPressed]
     *
     * TODO move this to the navigation interactor when it lands.
     */
    override fun onBackPressed(): Boolean {
        if (store.state.mode is Mode.Select) {
            store.dispatch(TabsTrayAction.ExitSelectMode)
            return true
        }
        return false
    }

    override fun getLayoutManagerForPosition(
        context: Context,
        position: Int
    ): RecyclerView.LayoutManager {
        if (position == TrayPagerAdapter.POSITION_SYNCED_TABS) {
            // Lists are just Grids with one column :)
            return GridLayoutManager(context, 1)
        }

        // Normal/Private tabs
        val numberOfColumns = if (settings.gridTabView) {
            context.numberOfGridColumns
        } else {
            1
        }

        return GridLayoutManager(context, numberOfColumns)
    }

    /**
     * See [BrowserTrayInteractor.onFabClicked]
     */
    override fun onFabClicked(isPrivate: Boolean) {
        controller.handleOpeningNewTab(isPrivate)
    }
}
