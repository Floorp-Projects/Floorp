/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngineManager
import org.mozilla.fenix.components.searchengine.FenixSearchEngineProvider
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Mockable

/**
 * Component group for all search engine integration related functionality.
 */
@Mockable
class Search(private val context: Context) {
    val provider = FenixSearchEngineProvider(context)

    /**
     * This component provides access to a centralized registry of search engines.
     */
    val searchEngineManager by lazy {
        SearchEngineManager(
            coroutineContext = IO,
            providers = listOf(provider)
        ).apply {
            registerForLocaleUpdates(context)
            GlobalScope.launch {
                defaultSearchEngine = getDefaultSearchEngineAsync(
                    context,
                    context.settings().defaultSearchEngineName
                )
            }
        }
    }
}
