package org.mozilla.fenix.components

import android.content.Context

/**
 * Provides access to all components.
 */
class Components(private val context: Context) {
    val core by lazy { Core(context) }
    val search by lazy { Search(context) }
    val useCases by lazy { UseCases(context, core.sessionManager, search.searchEngineManager) }
}