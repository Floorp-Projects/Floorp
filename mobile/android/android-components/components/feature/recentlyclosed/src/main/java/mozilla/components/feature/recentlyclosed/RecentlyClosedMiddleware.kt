/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store

/**
 * [Middleware] implementation for handling [RecentlyClosedAction]s and syncing the closed tabs in
 * [BrowserState.closedTabs] with the [RecentlyClosedTabsStorage].
 */
class RecentlyClosedMiddleware(
    private val storage: Lazy<Storage>,
    private val maxSavedTabs: Int,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<BrowserState, BrowserAction> {

    @Suppress("ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is UndoAction.ClearRecoverableTabs -> {
                if (action.tag == context.state.undoHistory.tag) {
                    // If the user has removed tabs and not invoked "undo" then let's save all non
                    // private tabs.
                    context.store.dispatch(
                        RecentlyClosedAction.AddClosedTabsAction(
                            context.state.undoHistory.tabs.filter { tab -> !tab.state.private },
                        ),
                    )
                }
            }
            is UndoAction.AddRecoverableTabs -> {
                if (context.state.undoHistory.tabs.isNotEmpty()) {
                    // If new tabs get added to the undo history and there were some previously
                    // then add them to the list of closed tabs now since they will never go through
                    // the clear call above.
                    context.store.dispatch(
                        RecentlyClosedAction.AddClosedTabsAction(
                            context.state.undoHistory.tabs.filter { tab -> !tab.state.private },
                        ),
                    )
                }
            }
            is RecentlyClosedAction.AddClosedTabsAction -> {
                addTabsToStorage(action.tabs)
            }
            is RecentlyClosedAction.RemoveAllClosedTabAction -> {
                removeAllTabs()
            }
            is RecentlyClosedAction.RemoveClosedTabAction -> {
                removeTab(action)
            }
            is InitAction -> {
                initializeRecentlyClosed(context.store)
            }
            else -> {
                // no-op
            }
        }

        next(action)

        pruneTabs(context.store)
    }

    private fun pruneTabs(store: Store<BrowserState, BrowserAction>) {
        if (store.state.closedTabs.size > maxSavedTabs) {
            store.dispatch(RecentlyClosedAction.PruneClosedTabsAction(maxSavedTabs))
        }
    }

    private fun initializeRecentlyClosed(
        store: Store<BrowserState, BrowserAction>,
    ) = scope.launch {
        storage.value.getTabs().collect { tabs ->
            store.dispatch(RecentlyClosedAction.ReplaceTabsAction(tabs))
        }
    }

    private fun addTabsToStorage(
        tabList: List<RecoverableTab>,
    ) = scope.launch {
        storage.value.addTabsToCollectionWithMax(
            tabList,
            maxSavedTabs,
        )
    }

    private fun removeTab(
        action: RecentlyClosedAction.RemoveClosedTabAction,
    ) = scope.launch {
        storage.value.removeTab(action.tab)
    }

    private fun removeAllTabs() = scope.launch {
        storage.value.removeAllTabs()
    }

    /**
     * Interface for a storage saving snapshots of recently closed tabs / sessions.
     */
    interface Storage {
        /**
         * Returns an observable list of recently closed tabs as List of [RecoverableTab]s.
         */
        suspend fun getTabs(): Flow<List<TabState>>

        /**
         * Removes the given saved [RecoverableTab].
         */
        suspend fun removeTab(recentlyClosedTab: TabState)

        /**
         * Removes all saved [RecoverableTab]s.
         */
        suspend fun removeAllTabs()

        /**
         * Adds up to [maxTabs] [TabSessionState]s to storage, and then prunes storage to keep only
         * the newest [maxTabs].
         */
        suspend fun addTabsToCollectionWithMax(tabs: List<RecoverableTab>, maxTabs: Int)
    }
}
