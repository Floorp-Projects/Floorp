/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.browser.state.search.SearchEngine

/**
 * Value type that represents the state of search.
 *
 * @property searchEngines The map of search engines (bundled and custom) to their respective IDs.
 * @property defaultSearchEngineId The ID of default [SearchEngine]
 */
data class SearchState(
    val searchEngines: Map<String, SearchEngine> = emptyMap(),
    val defaultSearchEngineId: String? = null
)
