/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

/**
 * Interface for a class that can provide a default search engine.
 *
 * This interface is a temporary workaround to allow applications to switch to the new API slowly.
 * Once all consuming apps have been migrated this interface will be removed and all components
 * will be migrated to use the state in `BrowserStore` directly.
 *
 * https://github.com/mozilla-mobile/android-components/issues/8686
*/
interface DefaultSearchEngineProvider {
    /**
     * Returns the default search engine for the user; or `null` if no default search engine is
     * available.
     */
    fun getDefaultSearchEngine(): SearchEngine?

    /**
     * Returns the default search engine for that user; or `null` if no default search engine is
     * available. Other than [getDefaultSearchEngine] this method may suspend.
     */
    suspend fun retrieveDefaultSearchEngine(): SearchEngine?
}
