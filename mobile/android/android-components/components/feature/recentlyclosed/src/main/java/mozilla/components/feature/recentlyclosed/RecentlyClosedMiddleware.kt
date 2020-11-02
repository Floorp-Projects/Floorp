/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ClosedTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.Engine
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import java.util.UUID

/**
 * [Middleware] implementation for handling [RecentlyClosedAction]s and syncing the closed tabs in
 * [BrowserState.closedTabs] with the [RecentlyClosedTabsStorage].
 */
class RecentlyClosedMiddleware(
    applicationContext: Context,
    private val maxSavedTabs: Int,
    private val engine: Engine,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO)
) : Middleware<BrowserState, BrowserAction> {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val recentlyClosedTabsStorage: RecentlyClosedTabsStorage by lazy {
        RecentlyClosedTabsStorage(applicationContext, engine = engine)
    }

    @Suppress("ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        when (action) {
            is TabListAction.RemoveAllNormalTabsAction -> {
                context.store.dispatch(
                    RecentlyClosedAction.AddClosedTabsAction(
                        context.state.normalTabs.toClosedTab()
                    )
                )
            }
            is TabListAction.RemoveAllTabsAction -> {
                context.store.dispatch(
                    RecentlyClosedAction.AddClosedTabsAction(
                        context.state.normalTabs.toClosedTab()
                    )
                )
            }
            is TabListAction.RemoveTabAction -> {
                val tab = context.state.findTab(action.tabId) ?: return
                if (!tab.content.private) {
                    context.store.dispatch(
                        RecentlyClosedAction.AddClosedTabsAction(
                            listOf(tab).toClosedTab()
                        )
                    )
                }
            }
            is TabListAction.RemoveTabsAction -> {
                val tabs = action.tabIds.mapNotNull { context.state.findTab(it) }
                    .filterNot { it.content.private }
                context.store.dispatch(
                    RecentlyClosedAction.AddClosedTabsAction(
                        tabs.toClosedTab()
                    )
                )
            }
            is RecentlyClosedAction.AddClosedTabsAction -> {
                addTabsToStorage(
                    action.tabs
                )
            }
            is RecentlyClosedAction.RemoveAllClosedTabAction -> {
                removeAllTabs()
            }
            is RecentlyClosedAction.RemoveClosedTabAction -> {
                removeTab(action)
            }
            is RecentlyClosedAction.InitializeRecentlyClosedState -> {
                initializeRecentlyClosed(context.store)
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

    private fun List<TabSessionState>.toClosedTab(): List<ClosedTab> {
        return this.map {
            ClosedTab(
                id = UUID.randomUUID().toString(),
                title = it.content.title,
                url = it.content.url,
                createdAt = System.currentTimeMillis(),
                engineSessionState = it.engineState.engineSessionState
            )
        }
    }

    private fun initializeRecentlyClosed(
        store: Store<BrowserState, BrowserAction>
    ) = scope.launch {
        recentlyClosedTabsStorage.getTabs().collect { tabs ->
            store.dispatch(RecentlyClosedAction.ReplaceTabsAction(tabs))
        }
    }

    private fun addTabsToStorage(
        tabList: List<ClosedTab>
    ) = scope.launch {
        recentlyClosedTabsStorage.addTabsToCollectionWithMax(
            tabList, maxSavedTabs
        )
    }

    private fun removeTab(
        action: RecentlyClosedAction.RemoveClosedTabAction
    ) = scope.launch {
        recentlyClosedTabsStorage.removeTab(action.tab)
    }

    private fun removeAllTabs() = scope.launch {
        recentlyClosedTabsStorage.removeAllTabs()
    }
}
