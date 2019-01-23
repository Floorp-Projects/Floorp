package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases

/**
 * Component group for all use cases. Use cases are provided by feature
 * modules and can be triggered by UI interactions.
 */
class UseCases(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val searchEngineManager: SearchEngineManager
) {

    val searchUseCases by lazy { SearchUseCases(context, searchEngineManager, sessionManager) }
    val sessionUseCases by lazy { SessionUseCases(sessionManager) }
}
