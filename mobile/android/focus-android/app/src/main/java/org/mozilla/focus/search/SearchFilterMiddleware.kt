/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] for modifying the loaded list of [SearchEngine]s.
 */
class SearchFilterMiddleware : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        if (action is SearchAction.SetSearchEnginesAction) {
            val newAction = action.copy(
                regionSearchEngines = action.regionSearchEngines.filterBing(),
            )

            next(newAction)
        } else {
            next(action)
        }
    }
}

private fun List<SearchEngine>.filterBing(): List<SearchEngine> {
    return filter { searchEngine -> searchEngine.id != "bing" }
}
