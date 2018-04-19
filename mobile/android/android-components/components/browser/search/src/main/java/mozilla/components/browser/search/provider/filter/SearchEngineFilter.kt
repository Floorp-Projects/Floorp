/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider.filter

import android.content.Context
import mozilla.components.browser.search.SearchEngine

/**
 * Interface for classes that want to filter the list of search engines a SearchEngineProvider
 * implementation loads.
 */
interface SearchEngineFilter {
    /**
     * Returns true if the given search engine should be returned by the provider or false if this
     * search engine should be ignored.
     */
    fun filter(context: Context, searchEngine: SearchEngine): Boolean
}
