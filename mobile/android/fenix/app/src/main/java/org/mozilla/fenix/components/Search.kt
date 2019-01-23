package org.mozilla.fenix.components

import android.content.Context
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngineManager


/**
 * Component group for all search engine integration related functionality.
 */
class Search(private val context: Context) {

    /**
     * This component provides access to a centralized registry of search engines.
     */
    val searchEngineManager by lazy {
        SearchEngineManager().apply {
            GlobalScope.launch {
                load(context).await()
            }
        }
    }
}
