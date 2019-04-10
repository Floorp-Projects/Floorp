/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import kotlinx.coroutines.coroutineScope
import mozilla.components.browser.search.provider.SearchEngineProvider
import mozilla.components.browser.search.provider.SearchEngineList

/**
 * SearchEngineProvider implementation to load user entered custom search engines.
 */
class CustomSearchEngineProvider : SearchEngineProvider {
    // Our version of ktlint enforces the wrong modifier order. We need to update the plugin: #2488
    /* ktlint-disable modifier-order */
    override suspend fun loadSearchEngines(context: Context): SearchEngineList = coroutineScope {
        SearchEngineList(CustomSearchEngineStore.loadCustomSearchEngines(context), null)
    }
}
