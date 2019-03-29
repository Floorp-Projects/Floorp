/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider

import android.content.Context
import mozilla.components.browser.search.SearchEngine

/**
 * Interface for classes that load search engines from a specific source.
 */
interface SearchEngineProvider {
    /**
     * Load search engines from this provider.
     */
    suspend fun loadSearchEngines(context: Context): SearchEngineList
}

/**
 * Data class providing an ordered list of search engines and a default search engine from a
 * specific source.
 */
data class SearchEngineList(

    /**
     * An ordered list of search engines.
     */
    val list: List<SearchEngine>,

    /**
     * The default search engine if the user has no preference.
     */
    val default: SearchEngine?
)
