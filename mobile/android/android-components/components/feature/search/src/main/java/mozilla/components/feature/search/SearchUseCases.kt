/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import android.content.Context
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

/**
 * Contains use cases related to the search feature.
 */
class SearchUseCases(
    private val context: Context,
    private val searchEngineManager: SearchEngineManager,
    private val sessionManager: SessionManager
) {

    class DefaultSearchUrlUseCase(
        private val context: Context,
        private val searchEngineManager: SearchEngineManager,
        private val sessionManager: SessionManager
    ) {

        /**
         * Triggers a search using the default search engine for the provided search terms.
         *
         * @param searchTerms the search terms.
         * @param session the session to use, or the currently selected session if none
         * is provided.
         */
        fun invoke(searchTerms: String, session: Session = sessionManager.selectedSessionOrThrow) {
            val searchEngine = searchEngineManager.getDefaultSearchEngine(context)
            val searchUrl = searchEngine.buildSearchUrl(searchTerms)

            session.searchTerms = searchTerms

            sessionManager.getOrCreateEngineSession(session).loadUrl(searchUrl)
        }
    }

    val defaultSearch: DefaultSearchUrlUseCase by lazy {
        DefaultSearchUrlUseCase(context, searchEngineManager, sessionManager)
    }
}
