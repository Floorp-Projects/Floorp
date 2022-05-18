/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.selection.SelectionInteractor
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayController
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.isSelect

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

    /**
     * Indicates Play/Pause item is clicked.
     * @param tab [TabSessionState] to close.
     */
    fun onMediaClicked(tab: TabSessionState)

    /**
     * Handles clicks when multi-selection is enabled.
     */
    fun onMultiSelectClicked(
        tab: TabSessionState,
        holder: SelectionHolder<TabSessionState>,
        source: String?
    )

    /**
     * Handles long click events when tab item is clicked.
     */
    fun onLongClicked(
        tab: TabSessionState,
        holder: SelectionHolder<TabSessionState>
    ): Boolean
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
) : BrowserTrayInteractor {

    private val selectTabWrapper by lazy {
        SelectTabUseCaseWrapper(selectTab) {
            trayInteractor.onBrowserTabSelected()
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

    /**
     * See [BrowserTrayInteractor.onMultiSelectClicked]
     */
    override fun onMediaClicked(tab: TabSessionState) {
        controller.handleMediaClicked(tab)
    }

    /**
     * See [BrowserTrayInteractor.onMultiSelectClicked]
     */
    override fun onMultiSelectClicked(
        tab: TabSessionState,
        holder: SelectionHolder<TabSessionState>,
        source: String?
    ) {
        val selected = holder.selectedItems
        when {
            selected.isEmpty() && store.state.mode.isSelect().not() -> {
                onTabSelected(tab, source)
            }
            tab.id in selected.map { it.id } -> deselect(tab)
            else -> select(tab)
        }
    }

    /**
     * See [BrowserTrayInteractor.onLongClicked]
     */
    override fun onLongClicked(
        tab: TabSessionState,
        holder: SelectionHolder<TabSessionState>
    ): Boolean {
        return if (holder.selectedItems.isEmpty()) {
            Collections.longPress.record(NoExtras())
            select(tab)
            true
        } else {
            false
        }
    }

    private fun selectTab(tab: TabSessionState, source: String? = null) {
        selectTabWrapper.invoke(tab.id, source)
    }

    private fun closeTab(tab: TabSessionState, source: String? = null) {
        trayInteractor.onDeleteTab(tab.id, source)
    }
}
