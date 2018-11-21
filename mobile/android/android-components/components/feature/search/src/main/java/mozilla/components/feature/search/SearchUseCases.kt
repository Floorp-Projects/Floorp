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

    class DefaultSearchUseCase(
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
            val searchUrl = searchEngineManager.getDefaultSearchEngine(context).buildSearchUrl(searchTerms)

            session.searchTerms = searchTerms

            sessionManager.getOrCreateEngineSession(session).loadUrl(searchUrl)
        }

        /**
         * Triggers a search on a new session, using the default search engine for the provided search terms.
         *
         * @param searchTerms the search terms.
         * @param selected whether or not the new session should be selected, defaults to true.
         * @param private whether or not the new session should be private, defaults to false.
         * @param source the source of the new session.
         */
        fun invoke(
            searchTerms: String,
            source: Session.Source,
            selected: Boolean = true,
            private: Boolean = false
        ) {
            val searchUrl = searchEngineManager.getDefaultSearchEngine(context).buildSearchUrl(searchTerms)

            val session = Session(searchUrl, private, source)
            session.searchTerms = searchTerms

            sessionManager.add(session, selected)
            sessionManager.getOrCreateEngineSession(session).loadUrl(searchUrl)
        }
    }

    val defaultSearch: DefaultSearchUseCase by lazy {
        DefaultSearchUseCase(context, searchEngineManager, sessionManager)
    }
}
