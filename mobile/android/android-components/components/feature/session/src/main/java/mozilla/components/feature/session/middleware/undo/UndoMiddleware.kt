/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.middleware.undo

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.recover.toRecoverableTab
import mozilla.components.browser.state.state.recover.toTabSessionStates
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.support.base.log.logger.Logger
import java.util.UUID
import mozilla.components.support.base.coroutines.Dispatchers as MozillaDispatchers

/**
 * [Middleware] implementation that adds removed tabs to [BrowserState.undoHistory] for a short
 * amount of time ([clearAfterMillis]). Dispatching [UndoAction.RestoreRecoverableTabs] will restore
 * the tabs from [BrowserState.undoHistory].
 */
class UndoMiddleware(
    private val clearAfterMillis: Long = 5000, // For comparison: a LENGTH_LONG Snackbar takes 2750.
    private val mainScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
    private val waitScope: CoroutineScope = CoroutineScope(MozillaDispatchers.Cached)
) : Middleware<BrowserState, BrowserAction> {
    private val logger = Logger("UndoMiddleware")
    private var clearJob: Job? = null

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        val state = context.state

        when (action) {
            // Remember removed tabs
            is TabListAction.RemoveAllNormalTabsAction -> onTabsRemoved(
                context, state.normalTabs, state.selectedTabId
            )
            is TabListAction.RemoveAllPrivateTabsAction -> onTabsRemoved(
                context, state.privateTabs, state.selectedTabId
            )
            is TabListAction.RemoveAllTabsAction -> {
                if (action.recoverable) {
                    onTabsRemoved(context, state.tabs, state.selectedTabId)
                }
            }
            is TabListAction.RemoveTabAction -> state.findTab(action.tabId)?.let {
                onTabsRemoved(context, listOf(it), state.selectedTabId)
            }
            is TabListAction.RemoveTabsAction -> {
                action.tabIds.mapNotNull { state.findTab(it) }.let {
                    onTabsRemoved(context, it, state.selectedTabId)
                }
            }

            // Restore
            is UndoAction.RestoreRecoverableTabs -> restore(context.store, context.state)
        }

        next(action)
    }

    private fun onTabsRemoved(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        tabs: List<SessionState>,
        selectedTabId: String?
    ) {
        clearJob?.cancel()

        val recoverableTabs = tabs.mapNotNull {
            it as? TabSessionState
        }.map {
            it.toRecoverableTab()
        }

        if (recoverableTabs.isEmpty()) {
            logger.debug("No recoverable tabs to add to undo history.")
            return
        }

        val tag = UUID.randomUUID().toString()

        val selectionToRestore = selectedTabId?.let { recoverableTabs.find { it.id == selectedTabId }?.id }

        context.dispatch(
            UndoAction.AddRecoverableTabs(tag, recoverableTabs, selectionToRestore)
        )

        val store = context.store

        clearJob = waitScope.launch {
            delay(clearAfterMillis)
            store.dispatch(UndoAction.ClearRecoverableTabs(tag))
        }
    }

    private fun restore(
        store: Store<BrowserState, BrowserAction>,
        state: BrowserState
    ) = mainScope.launch {
        clearJob?.cancel()

        // Since we have to restore into SessionManager (until we can nuke it from orbit and only use BrowserStore),
        // this is a bit crude. For example we do not restore into the previous position. The goal is to make this
        // nice once we can restore directly into BrowserState.

        val undoHistory = state.undoHistory
        val tabs = undoHistory.tabs
        if (tabs.isEmpty()) {
            logger.debug("No recoverable tabs for undo.")
            return@launch
        }

        store.dispatch(TabListAction.RestoreAction(tabs.toTabSessionStates()))

        // Restore the previous selection if needed.
        undoHistory.selectedTabId?.let { tabId ->
            store.dispatch(TabListAction.SelectTabAction(tabId))
        }
    }
}
