/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import android.content.Context
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

/**
 * Contains use cases related to the search feature.
 *
 * @param onNoSession When invoking a use case that requires a (selected) [Session] and when no [Session] is available
 * this (optional) lambda will be invoked to create a [Session]. The default implementation creates a [Session] and adds
 * it to the [SessionManager].
 */
class SearchUseCases(
    context: Context,
    searchEngineManager: SearchEngineManager,
    sessionManager: SessionManager,
    onNoSession: (String) -> Session = { url ->
        Session(url).apply { sessionManager.add(this) }
    }
) {
    interface SearchUseCase {
        fun invoke(searchTerms: String, searchEngine: SearchEngine? = null)
    }

    class DefaultSearchUseCase(
        private val context: Context,
        private val searchEngineManager: SearchEngineManager,
        private val sessionManager: SessionManager,
        private val onNoSession: (String) -> Session
    ) : SearchUseCase {
        /**
         * Triggers a search in the currently selected session.
         */
        override fun invoke(searchTerms: String, searchEngine: SearchEngine?) {
            invoke(searchTerms, sessionManager.selectedSession, searchEngine)
        }

        /**
         * Triggers a search using the default search engine for the provided search terms.
         *
         * @param searchTerms the search terms.
         * @param session the session to use, or the currently selected session if none
         * is provided.
         * @param searchEngine Search Engine to use, or the default search engine if none is provided
         */
        operator fun invoke(
            searchTerms: String,
            session: Session? = sessionManager.selectedSession,
            searchEngine: SearchEngine? = null
        ) {
            val searchUrl = searchEngine?.let {
                searchEngine.buildSearchUrl(searchTerms)
            } ?: searchEngineManager.getDefaultSearchEngine(context).buildSearchUrl(searchTerms)

            val searchSession = session ?: onNoSession.invoke(searchUrl)

            searchSession.searchTerms = searchTerms

            sessionManager.getOrCreateEngineSession(searchSession).loadUrl(searchUrl)
        }
    }

    class NewTabSearchUseCase(
        private val context: Context,
        private val searchEngineManager: SearchEngineManager,
        private val sessionManager: SessionManager,
        private val isPrivate: Boolean
    ) : SearchUseCase {
        override fun invoke(searchTerms: String, searchEngine: SearchEngine?) {
            invoke(
                searchTerms,
                source = Session.Source.NONE,
                selected = true,
                private = isPrivate,
                searchEngine = searchEngine
            )
        }

        /**
         * Triggers a search on a new session, using the default search engine for the provided search terms.
         *
         * @param searchTerms the search terms.
         * @param selected whether or not the new session should be selected, defaults to true.
         * @param private whether or not the new session should be private, defaults to false.
         * @param source the source of the new session.
         * @param searchEngine Search Engine to use, or the default search engine if none is provided
         */
        operator fun invoke(
            searchTerms: String,
            source: Session.Source,
            selected: Boolean = true,
            private: Boolean = false,
            searchEngine: SearchEngine? = null
        ) {
            val searchUrl = searchEngine?.let {
                searchEngine.buildSearchUrl(searchTerms)
            } ?: searchEngineManager.getDefaultSearchEngine(context).buildSearchUrl(searchTerms)

            val session = Session(searchUrl, private, source)
            session.searchTerms = searchTerms

            sessionManager.add(session, selected)
            sessionManager.getOrCreateEngineSession(session).loadUrl(searchUrl)
        }
    }

    val defaultSearch: DefaultSearchUseCase by lazy {
        DefaultSearchUseCase(context, searchEngineManager, sessionManager, onNoSession)
    }

    val newTabSearch: NewTabSearchUseCase by lazy {
        NewTabSearchUseCase(context, searchEngineManager, sessionManager, false)
    }

    val newPrivateTabSearch: NewTabSearchUseCase by lazy {
        NewTabSearchUseCase(context, searchEngineManager, sessionManager, true)
    }
}
