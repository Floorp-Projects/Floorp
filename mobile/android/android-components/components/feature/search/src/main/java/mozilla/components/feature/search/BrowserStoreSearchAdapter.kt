/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.search.SearchRequest

/**
 * Adapter which wraps a [browserStore] in order to fulfill the [SearchAdapter] interface.
 *
 * This class uses the [browserStore] to determine when private mode is active, and updates the
 * [browserStore] whenever a new search has been requested.
 *
 * NOTE: this will add [SearchRequest]s to [browserStore.state] when [sendSearch] is called. Client
 * code is responsible for consuming these requests and displaying something to the user.
 *
 * NOTE: client code is responsible for sending [ContentAction.ConsumeSearchRequestAction]s
 * after consuming events. See [SearchFeature] for a component that will handle this for you.
 */
class BrowserStoreSearchAdapter(private val browserStore: BrowserStore) : SearchAdapter {

    override fun sendSearch(isPrivate: Boolean, text: String) {
        val selectedTabId = browserStore.state.selectedTabId ?: return
        browserStore.dispatch(ContentAction.UpdateSearchRequestAction(selectedTabId, SearchRequest(isPrivate, text)))
    }

    override fun isPrivateSession(): Boolean = browserStore.state.selectedTab?.content?.private ?: false
}
