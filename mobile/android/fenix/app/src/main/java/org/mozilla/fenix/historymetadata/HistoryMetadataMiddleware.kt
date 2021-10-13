/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.EngineAction
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

    // Tracks whether a page load is in progress that was triggered directly by the app
    // e.g. via the toolbar as opposed to via web content.
    private var directLoadTriggered: Boolean = false

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
            is ContentAction.UpdateUrlAction -> {
                context.state.findNormalTab(action.sessionId)?.let { tab ->
                    val selectedTab = tab.id == context.state.selectedTabId
                    // When page url changes (e.g. user navigated away by clicking on a link)
                    // we update metadata for the selected (i.e. previous) url of this tab.
                    // We don't update metadata for cases or reload or restore.
                    // In case of a reload it's not necessary - metadata will be updated when
                    // user moves away from the page or tab.
                    // In case of restore, it's both unnecessary (like for a reload) and
                    // problematic, since our lastAccess time will be from before the tab was
                    // restored, resulting in an incorrect (too long) viewTime observation, as if
                    // the user was looking at the page while the browser wasn't even running.
                    if (selectedTab && action.url != tab.content.url) {
                        updateHistoryMetadata(tab)
                    }
                }
            }
            is EngineAction.LoadUrlAction,
            is EngineAction.OptimizedLoadUrlTriggeredAction -> {
                // This isn't an ideal fix as we shouldn't have to hold any state in the middleware:
                // https://github.com/mozilla-mobile/android-components/issues/11034
                directLoadTriggered = true
            }
        }

        next(action)

        // Post process actions. At this point, tab state will be up-to-date and will possess any
        // changes introduced by the action. These handlers rely on up-to-date tab state, which
        // is why they're in the "post" section.
        when (action) {
            is TabListAction.AddTabAction -> {
                if (!action.tab.content.private) {
                    createHistoryMetadataIfNeeded(context, action.tab)
                }
            }
            // NB: sometimes this fires multiple times after the page finished loading.
            is ContentAction.UpdateHistoryStateAction -> {
                context.state.findNormalTab(action.sessionId)?.let { tab ->
                    createHistoryMetadataIfNeeded(context, tab)
                }

                // Once we get a history update let's reset the flag for future loads.
                directLoadTriggered = false
            }
            // NB: this could be called bunch of times in quick succession.
            is MediaSessionAction.UpdateMediaMetadataAction -> {
                context.state.findNormalTab(action.tabId)?.let { tab ->
                    createHistoryMetadata(context, tab)
                }
            }
        }
    }

    private fun createHistoryMetadataIfNeeded(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        tab: TabSessionState
    ) {
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

    private fun createHistoryMetadata(context: MiddlewareContext<BrowserState, BrowserAction>, tab: TabSessionState) {
        val tabParent = tab.getParent(context.store)
        val previousUrlIndex = tab.content.history.currentIndex - 1
        val tabMetadataHasSearchTerms = !tab.historyMetadata?.searchTerm.isNullOrBlank()

        // Obtain search terms and referrer url either from tab parent, from the history stack, or
        // from the tab itself.
        // At a high level, there are two main cases here - 1) either the tab was opened as a 'new tab'
        // via the search results page, or 2) a page was opened in the same tab as the search results page.
        // Details about the New Tab case:
        // - we obtain search terms via tab's parent (the search results page)
        // - however, it's possible that parent changed (e.g. user navigated away from the search
        // results page).
        // - our approach below is to capture search terms from the parent within the tab.historyMetadata
        // state on the first load of the tab, and then rely on this data for subsequent page loads on that tab.
        // - this way, once a tab becomes part of the search group, it won't leave this search group
        // unless a direct navigation event happens.
        val (searchTerm, referrerUrl) = when {
            // Loading page opened in a New Tab for the first time.
            tabParent != null && !tabMetadataHasSearchTerms -> {
                val searchTerms = tabParent.content.searchTerms.takeUnless { it.isEmpty() }
                    ?: context.state.search.parseSearchTerms(tabParent.content.url)
                searchTerms to tabParent.content.url
            }
            // We only want to inspect the previous url in history if the user navigated via
            // web content i.e., they followed a link, not if the user navigated directly via
            // toolbar.
            !directLoadTriggered && previousUrlIndex >= 0 -> {
                // Once a tab is within the search group, only a direct load event (via the toolbar) can change that.
                val (searchTerms, referrerUrl) = if (tabMetadataHasSearchTerms) {
                    tab.historyMetadata?.searchTerm to tab.historyMetadata?.referrerUrl
                } else {
                    val previousUrl = tab.content.history.items[previousUrlIndex].uri
                    context.state.search.parseSearchTerms(previousUrl) to previousUrl
                }

                if (searchTerms != null) {
                    searchTerms to referrerUrl
                } else {
                    null to null
                }
            }
            // In certain redirect cases, we won't have a previous url in the history stack of the tab,
            // but will have the search terms already set on the tab from having gone through this logic
            // for the redirecting url. So we leave this tab within the search group it's already in
            // unless a new direct load (via the toolbar) was triggered.
            tabMetadataHasSearchTerms && !(directLoadTriggered && previousUrlIndex >= 0) -> {
                tab.historyMetadata?.searchTerm to tab.historyMetadata?.referrerUrl
            }
            // We had no search terms, no history stack, and no parent.
            // This would be the case for any page loaded directly via the toolbar including
            // a search results page itself. For now, the original search results page is not
            // part of the search group: https://github.com/mozilla-mobile/fenix/issues/21659.
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
