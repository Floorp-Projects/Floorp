/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store

/**
 * This [Middleware] reacts to various browsing events and records history metadata as needed.
 */
class HistoryMetadataMiddleware(
    private val historyMetadataService: HistoryMetadataService
) : Middleware<BrowserState, BrowserAction> {

    @Suppress("ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        // Pre process actions
        when (action) {
            is TabListAction.AddTabAction -> {
                if (action.select) {
                    // Before we add and select a new tab we update the metadata
                    // of the currently selected tab, if not private.
                    context.state.selectedNormalTab?.let {
                        updateHistoryMetadata(it)
                    }
                }
            }
            is TabListAction.SelectTabAction -> {
                // Before we select a new tab we update the metadata
                // of the currently selected tab, if not private.
                context.state.selectedNormalTab?.let {
                    updateHistoryMetadata(it)
                }
            }
            is TabListAction.RemoveTabAction -> {
                if (action.tabId == context.state.selectedTabId) {
                    context.state.findNormalTab(action.tabId)?.let {
                        updateHistoryMetadata(it)
                    }
                }
            }
            is TabListAction.RemoveTabsAction -> {
                action.tabIds.find { it == context.state.selectedTabId }?.let {
                    context.state.findNormalTab(it)?.let { tab ->
                        updateHistoryMetadata(tab)
                    }
                }
            }
            is ContentAction.UpdateLoadingStateAction -> {
                context.state.findNormalTab(action.sessionId)?.let { tab ->
                    val selectedTab = tab.id == context.state.selectedTabId
                    if (tab.content.loading && !action.loading) {
                        // When a page stops loading we record its metadata
                        createHistoryMetadata(context, tab)
                    } else if (!tab.content.loading && action.loading && selectedTab) {
                        // When a page starts loading (e.g. user navigated away by
                        // clicking on a link) we update metadata
                        updateHistoryMetadata(tab)
                    }
                }
            }
        }

        next(action)

        // Post process actions
        when (action) {
            // We're handling this after processing the action because we want the tab
            // state to contain the updated media session state.
            is MediaSessionAction.UpdateMediaMetadataAction -> {
                context.state.findNormalTab(action.tabId)?.let { tab ->
                    createHistoryMetadata(context, tab)
                }
            }
        }
    }

    private fun createHistoryMetadata(context: MiddlewareContext<BrowserState, BrowserAction>, tab: TabSessionState) {
        val key = historyMetadataService.createMetadata(tab, tab.getParent(context.store))
        context.dispatch(HistoryMetadataAction.SetHistoryMetadataKeyAction(tab.id, key))
    }

    private fun updateHistoryMetadata(tab: TabSessionState) {
        tab.historyMetadata?.let {
            historyMetadataService.updateMetadata(it, tab)
        }
    }

    private fun TabSessionState.getParent(store: Store<BrowserState, BrowserAction>): TabSessionState? {
        return parentId?.let {
            store.state.findTab(it)
        }
    }
}

/**
 * Finds and returns the normal (non-private) tab with the given id. Returns null if no
 * matching tab could be found.
 *
 * @param tabId The ID of the tab to search for.
 * @return The [TabSessionState] with the provided [tabId] or null if it could not be found.
 */
private fun BrowserState.findNormalTab(tabId: String): TabSessionState? {
    return normalTabs.firstOrNull { it.id == tabId }
}

/**
 * The currently selected tab if there's one that is not private.
 */
private val BrowserState.selectedNormalTab: TabSessionState?
    get() = selectedTabId?.let { id -> findNormalTab(id) }
