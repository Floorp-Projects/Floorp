/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.provider.filter.SearchEngineFilter

/**
 * SearchEngineFilter implementation to hide Bing from the default list.
 */
class BingSearchEngineFilter : SearchEngineFilter {
    override fun filter(context: Context, searchEngine: SearchEngine): Boolean =
            searchEngine.identifier != "bing"
}
