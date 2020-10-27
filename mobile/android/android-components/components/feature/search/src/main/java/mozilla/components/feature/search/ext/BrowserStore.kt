/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import mozilla.components.browser.search.DefaultSearchEngineProvider
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.Store

/**
 * Converts a [BrowserStore] to follow the [DefaultSearchEngineProvider] interface.
 *
 * This method is a temporary workaround to allow applications to switch to the new API slowly.
 * Once all consuming apps have been migrated this extension will be removed and all components
 * will be migrated to use [BrowserStore] directly.
 *
 * https://github.com/mozilla-mobile/android-components/issues/8686
 */
fun BrowserStore.toDefaultSearchEngineProvider() = object : DefaultSearchEngineProvider {
    override fun getDefaultSearchEngine(): SearchEngine? {
        return state.search.selectedOrDefaultSearchEngine?.legacy()
    }

    override suspend fun retrieveDefaultSearchEngine(): SearchEngine? {
        return getDefaultSearchEngine()
    }
}

/**
 * Waits (asynchronously, non-blocking) for the search state to be loaded from disk (when using
 * `RegionMiddleware` and `SearchMiddleware`) and invokes [block] with the default search engine
 * (or `null` if no default could be loaded).
 */
fun BrowserStore.waitForSelectedOrDefaultSearchEngine(
    block: (mozilla.components.browser.state.search.SearchEngine?) -> Unit
) {
    // Did we already load the search state? In that case we can invoke `block` immediately.
    if (state.search.complete) {
        block(state.search.selectedOrDefaultSearchEngine)
        return
    }

    // Otherwise: Wait for the search state to be loaded and then invoke `block`.
    var subscription: Store.Subscription<BrowserState, BrowserAction>? = null
    subscription = observeManually { state ->
        if (state.search.complete) {
            block(state.search.selectedOrDefaultSearchEngine)
            subscription!!.unsubscribe()
        }
    }
    subscription.resume()
}
