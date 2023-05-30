/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.ext

import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.availableSearchEngines

/**
 * The list of search engine shortcuts to be available for quick search menu.
 */
val SearchState.searchEngineShortcuts: List<SearchEngine>
    get() = (
        regionSearchEngines + additionalSearchEngines + availableSearchEngines +
            customSearchEngines + applicationSearchEngines
        ).filter {
        !disabledSearchEngineIds.contains(it.id)
    }
