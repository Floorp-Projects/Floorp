/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.search.SearchRequest
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.support.utils.ext.toNullablePair

/**
 * Lifecycle aware feature that forwards [SearchRequest]s from the [store] to [performSearch], and
 * then consumes them.
 *
 * NOTE: that this only consumes SearchRequests, and will not hook up the [store] to a source of
 * SearchRequests. See [mozilla.components.concept.engine.selection.SelectionActionDelegate] for use
 * in generating SearchRequests.
 */
class SearchFeature(
    private val store: BrowserStore,
    private val tabId: String? = null,
    private val performSearch: (SearchRequest, tabId: String) -> Unit,
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    override fun start() {
        scope = store.flowScoped { flow ->
            flow.map { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .ifChanged { it?.content?.searchRequest }
                // Do nothing if searchRequest or sessionId is null
                .mapNotNull { tab -> Pair(tab?.content?.searchRequest, tab?.id).toNullablePair() }
                .collect { (searchRequest, sessionId) ->
                    performSearch(searchRequest, sessionId)
                    store.dispatch(ContentAction.ConsumeSearchRequestAction(sessionId))
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
