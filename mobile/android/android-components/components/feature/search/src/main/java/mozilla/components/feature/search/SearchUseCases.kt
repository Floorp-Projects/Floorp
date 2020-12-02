/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import mozilla.components.browser.search.DefaultSearchEngineProvider
import mozilla.components.browser.search.SearchEngine as LegacySearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.log.logger.Logger

/**
 * Contains use cases related to the search feature.
 *
 * @param onNoSession When invoking a use case that requires a (selected) [Session] and when no [Session] is available
 * this (optional) lambda will be invoked to create a [Session]. The default implementation creates a [Session] and adds
 * it to the [SessionManager].
 */
class SearchUseCases(
    store: BrowserStore,
    defaultSearchEngineProvider: DefaultSearchEngineProvider,
    sessionManager: SessionManager,
    onNoSession: (String) -> Session = { url ->
        Session(url).apply { sessionManager.add(this) }
    }
) {
    interface SearchUseCase {
        /**
         * Triggers a search.
         */
        fun invoke(
            searchTerms: String,
            searchEngine: LegacySearchEngine? = null,
            parentSessionId: String? = null
        )
    }

    class DefaultSearchUseCase(
        private val store: BrowserStore,
        private val defaultSearchEngineProvider: DefaultSearchEngineProvider,
        private val sessionManager: SessionManager,
        private val onNoSession: (String) -> Session
    ) : SearchUseCase {
        private val logger = Logger("DefaultSearchUseCase")

        /**
         * Triggers a search in the currently selected session.
         */
        override fun invoke(
            searchTerms: String,
            searchEngine: LegacySearchEngine?,
            parentSessionId: String?
        ) {
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
            searchEngine: LegacySearchEngine? = null
        ) {
            val searchUrl = searchEngine?.let {
                searchEngine.buildSearchUrl(searchTerms)
            } ?: defaultSearchEngineProvider.getDefaultSearchEngine()?.buildSearchUrl(searchTerms)

            if (searchUrl == null) {
                logger.warn("No default search engine available to perform search")
                return
            }

            val searchSession = session ?: onNoSession.invoke(searchUrl)

            searchSession.searchTerms = searchTerms

            store.dispatch(EngineAction.LoadUrlAction(
                searchSession.id,
                searchUrl
            ))
        }
    }

    class NewTabSearchUseCase(
        private val store: BrowserStore,
        private val defaultSearchEngineProvider: DefaultSearchEngineProvider,
        private val sessionManager: SessionManager,
        private val isPrivate: Boolean
    ) : SearchUseCase {
        private val logger = Logger("NewTabSearchUseCase")

        override fun invoke(
            searchTerms: String,
            searchEngine: LegacySearchEngine?,
            parentSessionId: String?
        ) {
            invoke(
                searchTerms,
                source = SessionState.Source.NONE,
                selected = true,
                private = isPrivate,
                searchEngine = searchEngine,
                parentSessionId = parentSessionId
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
         * @param parentSession optional parent session to attach this new search session to
         */
        @Suppress("LongParameterList")
        operator fun invoke(
            searchTerms: String,
            source: SessionState.Source,
            selected: Boolean = true,
            private: Boolean = false,
            searchEngine: LegacySearchEngine? = null,
            parentSessionId: String? = null
        ) {
            val searchUrl = searchEngine?.let {
                searchEngine.buildSearchUrl(searchTerms)
            } ?: defaultSearchEngineProvider.getDefaultSearchEngine()?.buildSearchUrl(searchTerms)

            if (searchUrl == null) {
                logger.warn("No default search engine available to perform search")
                return
            }

            val session = Session(searchUrl, private, source)
            session.searchTerms = searchTerms

            val parentSession = parentSessionId?.let { sessionManager.findSessionById(it) }

            sessionManager.add(session, selected, parent = parentSession)

            store.dispatch(EngineAction.LoadUrlAction(
                session.id,
                searchUrl
            ))
        }
    }

    /**
     * Adds a new search engine to the list of search engines the user can use for searches.
     */
    class AddNewSearchEngineUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Adds the given [searchEngine] to the list of search engines the user can use for searches.
         */
        operator fun invoke(
            searchEngine: SearchEngine
        ) {
            when (searchEngine.type) {
                SearchEngine.Type.BUNDLED -> store.dispatch(
                    SearchAction.ShowSearchEngineAction(searchEngine.id)
                )

                SearchEngine.Type.BUNDLED_ADDITIONAL -> store.dispatch(
                    SearchAction.AddAdditionalSearchEngineAction(searchEngine.id)
                )

                SearchEngine.Type.CUSTOM -> store.dispatch(
                    SearchAction.UpdateCustomSearchEngineAction(searchEngine)
                )
            }
        }
    }

    /**
     * Removes a search engine from the list of search engines the user can use for searches.
     */
    class RemoveExistingSearchEngineUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Removes the given [searchEngine] from the list of search engines the user can use for
         * searches.
         */
        operator fun invoke(
            searchEngine: SearchEngine
        ) {
            when (searchEngine.type) {
                SearchEngine.Type.BUNDLED -> store.dispatch(
                    SearchAction.HideSearchEngineAction(searchEngine.id)
                )

                SearchEngine.Type.BUNDLED_ADDITIONAL -> store.dispatch(
                    SearchAction.RemoveAdditionalSearchEngineAction(searchEngine.id)
                )

                SearchEngine.Type.CUSTOM -> store.dispatch(
                    SearchAction.RemoveCustomSearchEngineAction(searchEngine.id)
                )
            }
        }
    }

    /**
     * Marks a search engine as "selected" by the user to be the default search engine to perform
     * searches with.
     */
    class SelectSearchEngineUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Marks the given [searchEngine] as "selected" by the user to be the default search engine
         * to perform searches with.
         */
        operator fun invoke(
            searchEngine: SearchEngine
        ) {
            val name = if (searchEngine.type == SearchEngine.Type.BUNDLED) {
                // For bundled search engines we additionally save the name of the search engine.
                // We do this because with "home" region changes the previous search plugin/id
                // may no longer be available, but there may be a clone of the search engine with
                // a different plugin/id using the same name.
                // This should be safe to do since Fenix as well as Fennec only kept the name of
                // the default search engine.
                // For all other cases (e.g. custom search engines) we only care about the ID and
                // do not want to switch to a different search engine based on its name once it is
                // gone.
                searchEngine.name
            } else {
                null
            }

            store.dispatch(
                SearchAction.SelectSearchEngineAction(searchEngine.id, name)
            )
        }
    }

    val defaultSearch: DefaultSearchUseCase by lazy {
        DefaultSearchUseCase(store, defaultSearchEngineProvider, sessionManager, onNoSession)
    }

    val newTabSearch: NewTabSearchUseCase by lazy {
        NewTabSearchUseCase(store, defaultSearchEngineProvider, sessionManager, false)
    }

    val newPrivateTabSearch: NewTabSearchUseCase by lazy {
        NewTabSearchUseCase(store, defaultSearchEngineProvider, sessionManager, true)
    }

    val addSearchEngine: AddNewSearchEngineUseCase by lazy {
        AddNewSearchEngineUseCase(store)
    }

    val removeSearchEngine: RemoveExistingSearchEngineUseCase by lazy {
        RemoveExistingSearchEngineUseCase(store)
    }

    val selectSearchEngine: SelectSearchEngineUseCase by lazy {
        SelectSearchEngineUseCase(store)
    }
}
