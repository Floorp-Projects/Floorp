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
    suspend fun loadSearchEngines(context: Context): List<SearchEngine>
}
