/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import mozilla.components.browser.search.DefaultSearchEngineProvider
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.state.state.defaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore

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
        return state.search.defaultSearchEngine?.legacy()
    }

    override suspend fun retrieveDefaultSearchEngine(): SearchEngine? {
        return getDefaultSearchEngine()
    }
}
