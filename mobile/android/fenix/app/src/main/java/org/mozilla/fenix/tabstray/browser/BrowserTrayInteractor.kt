/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.selection.SelectionInteractor
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.tabstray.TabsTrayController
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * For interacting with UI that is specifically for [AbstractBrowserTrayList] and other browser
 * tab tray views.
 */
interface BrowserTrayInteractor : SelectionInteractor<TabSessionState>, UserInteractionHandler, TabsTray.Delegate {

    /**
     * Open a tab.
     *
     * @param tab [TabSessionState] to open in browser.
     * @param source app feature from which the [tab] was opened.
     */
    fun open(tab: TabSessionState, source: String? = null)

    /**
     * Close the tab.
     *
     * @param tab [TabSessionState] to close.
     * @param source app feature from which the [tab] was closed.
     */
    fun close(tab: TabSessionState, source: String? = null)

    /**
     * TabTray's Floating Action Button clicked.
     */
    fun onFabClicked(isPrivate: Boolean)

    /**
     * Recently Closed item is clicked.
     */
    fun onRecentlyClosedClicked()
}

/**
 * A default implementation of [BrowserTrayInteractor].
 */
@Suppress("TooManyFunctions")
class DefaultBrowserTrayInteractor(
    private val store: TabsTrayStore,
    private val trayInteractor: TabsTrayInteractor,
    private val controller: TabsTrayController,
    private val selectTab: TabsUseCases.SelectTabUseCase,
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
    override fun open(item: TabSessionState) {
        open(item, null)
    }

    /**
     * See [BrowserTrayInteractor.open].
     */
    override fun open(tab: TabSessionState, source: String?) {
        selectTab(tab, source)
    }

    /**
     * See [BrowserTrayInteractor.close].
     */
    override fun close(tab: TabSessionState, source: String?) {
        closeTab(tab, source)
    }

    /**
     * See [SelectionInteractor.select]
     */
    override fun select(item: TabSessionState) {
        store.dispatch(TabsTrayAction.AddSelectTab(item))
    }

    /**
     * See [SelectionInteractor.deselect]
     */
    override fun deselect(item: TabSessionState) {
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

    override fun onTabClosed(tab: TabSessionState, source: String?) {
        closeTab(tab, source)
    }

    override fun onTabSelected(tab: TabSessionState, source: String?) {
        selectTab(tab, source)
    }

    /**
     * See [BrowserTrayInteractor.onFabClicked]
     */
    override fun onFabClicked(isPrivate: Boolean) {
        controller.handleOpeningNewTab(isPrivate)
    }

    /**
     * See [BrowserTrayInteractor.onRecentlyClosedClicked]
     */
    override fun onRecentlyClosedClicked() {
        controller.handleNavigateToRecentlyClosed()
    }

    private fun selectTab(tab: TabSessionState, source: String? = null) {
        selectTabWrapper.invoke(tab.id, source)
    }

    private fun closeTab(tab: TabSessionState, source: String? = null) {
        removeTabWrapper.invoke(tab.id, source)
    }
}
