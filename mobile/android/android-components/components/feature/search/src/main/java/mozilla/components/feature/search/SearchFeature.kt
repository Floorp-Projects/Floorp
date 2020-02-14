/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.search.SearchRequest
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.utils.ext.toNullablePair

/**
 * Lifecycle aware feature that forwards [SearchRequest]s from the [store] to [performSearch], and
 * then consumes them.
 *
 * NOTE: that this only consumes SearchRequests, and will not hook up the [store] to a source of
 * SearchRequests. See [SelectionActionDelegate] for use in generating SearchRequests.
 */
class SearchFeature(
    private val store: BrowserStore,
    private val performSearch: (SearchRequest) -> Unit
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null

    override fun start() {
        scope = store.flowScoped { flow ->
            flow
                .map { it.selectedTab?.content?.searchRequest to it.selectedTabId }
                // Do nothing if searchRequest or sessionId is null
                .mapNotNull { pair -> pair.toNullablePair() }
                // We may see repeat values if other state changes before we handle the request.
                // Filter these out.
                .distinctUntilChangedBy { (searchRequest, _) -> searchRequest }
                .collect { (searchRequest, sessionId) ->
                    performSearch(searchRequest)
                    store.dispatch(ContentAction.ConsumeSearchRequestAction(sessionId))
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
