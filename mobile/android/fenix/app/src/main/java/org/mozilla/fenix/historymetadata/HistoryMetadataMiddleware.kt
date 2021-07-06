/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findNormalTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedNormalTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.search.ext.parseSearchTerms
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.support.base.log.logger.Logger

/**
 * This [Middleware] reacts to various browsing events and records history metadata as needed.
 */
class HistoryMetadataMiddleware(
    private val historyMetadataService: HistoryMetadataService
) : Middleware<BrowserState, BrowserAction> {

    private val logger = Logger("HistoryMetadataMiddleware")

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
                    if (!tab.content.loading && action.loading && selectedTab) {
                        // When a page starts loading (e.g. user navigated away by
                        // clicking on a link) we update metadata for the selected
                        // (i.e. previous) url of this tab.
                        updateHistoryMetadata(tab)
                    }
                }
            }
        }

        next(action)

        // Post process actions. At this point, tab state will be up-to-date and will possess any
        // changes introduced by the action. These handlers rely on up-to-date tab state, which
        // is why they're in the "post" section.
        when (action) {
            // NB: sometimes this fires multiple times after the page finished loading.
            is ContentAction.UpdateHistoryStateAction -> {
                context.state.findNormalTab(action.sessionId)?.let { tab ->
                    // When history state is ready, we can record metadata for this page.
                    val knownHistoryMetadata = tab.historyMetadata
                    val metadataPresentForUrl = knownHistoryMetadata != null &&
                            knownHistoryMetadata.url == tab.content.url
                    // Record metadata for tab if there is no metadata present, or if url of the
                    // tab changes since we last recorded metadata.
                    if (!metadataPresentForUrl) {
                        createHistoryMetadata(context, tab)
                    }
                }
            }
            // NB: this could be called bunch of times in quick succession.
            is MediaSessionAction.UpdateMediaMetadataAction -> {
                context.state.findNormalTab(action.tabId)?.let { tab ->
                    createHistoryMetadata(context, tab)
                }
            }
        }
    }

    private fun createHistoryMetadata(context: MiddlewareContext<BrowserState, BrowserAction>, tab: TabSessionState) {
        val tabParent = tab.getParent(context.store)
        val previousUrlIndex = tab.content.history.currentIndex - 1

        // Obtain search terms and referrer url either from tab parent, or from the history stack.
        val (searchTerm, referrerUrl) = when {
            tabParent != null -> {
                val searchTerms = tabParent.content.searchTerms.takeUnless { it.isEmpty() }
                    ?: context.state.search.parseSearchTerms(tabParent.content.url)
                searchTerms to tabParent.content.url
            }
            previousUrlIndex >= 0 -> {
                val previousUrl = tab.content.history.items[previousUrlIndex].uri
                val searchTerms = context.state.search.parseSearchTerms(previousUrl)
                if (searchTerms != null) {
                    searchTerms to previousUrl
                } else {
                    null to null
                }
            }
            else -> null to null
        }

        // Sanity check to make sure we don't record a metadata record referring to itself.
        if (tab.content.url == referrerUrl) {
            logger.debug("Current url same as referrer. Skipping metadata recording.")
            return
        }

        val key = historyMetadataService.createMetadata(tab, searchTerm, referrerUrl)
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
