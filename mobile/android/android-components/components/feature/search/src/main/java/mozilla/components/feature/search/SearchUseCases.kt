/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.ext.buildSearchUrl
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.base.log.logger.Logger

/**
 * Contains use cases related to the search feature.
 */
class SearchUseCases(
    store: BrowserStore,
    tabsUseCases: TabsUseCases,
    sessionUseCases: SessionUseCases,
) {
    interface SearchUseCase {
        /**
         * Triggers a search.
         */
        fun invoke(
            searchTerms: String,
            searchEngine: SearchEngine? = null,
            parentSessionId: String? = null,
        )
    }

    class DefaultSearchUseCase(
        private val store: BrowserStore,
        private val tabsUseCases: TabsUseCases,
        private val sessionUseCases: SessionUseCases,
    ) : SearchUseCase {
        private val logger = Logger("DefaultSearchUseCase")

        /**
         * Triggers a search in the currently selected session.
         */
        override fun invoke(
            searchTerms: String,
            searchEngine: SearchEngine?,
            parentSessionId: String?,
        ) {
            invoke(searchTerms, store.state.selectedTabId, searchEngine)
        }

        /**
         * Triggers a search using the default search engine for the provided search terms.
         *
         * @param searchTerms the search terms.
         * @param sessionId the ID of the session/tab to use, or null if the currently selected tab
         * should be used.
         * @param searchEngine Search Engine to use, or the default search engine if none is provided
         */
        operator fun invoke(
            searchTerms: String,
            sessionId: String? = store.state.selectedTabId,
            searchEngine: SearchEngine? = null,
        ) {
            val searchUrl = searchEngine?.let {
                searchEngine.buildSearchUrl(searchTerms)
            } ?: store.state.search.selectedOrDefaultSearchEngine?.buildSearchUrl(searchTerms)

            if (searchUrl == null) {
                logger.warn("No default search engine available to perform search")
                return
            }

            val id = if (sessionId == null) {
                // If no `sessionId` was passed in then create a new tab
                tabsUseCases.addTab(searchUrl)
            } else {
                // If we got a `sessionId` then try to find the tab and load the search URL in it
                val existingTab = store.state.findTabOrCustomTab(sessionId)
                if (existingTab != null) {
                    sessionUseCases.loadUrl(searchUrl, sessionId = existingTab.id)
                    existingTab.id
                } else {
                    // If the tab with the provided id was not found then create a new tab
                    tabsUseCases.addTab(searchUrl)
                }
            }

            store.dispatch(ContentAction.UpdateSearchTermsAction(id, searchTerms))
        }
    }

    class NewTabSearchUseCase(
        private val store: BrowserStore,
        private val tabsUseCases: TabsUseCases,
        private val isPrivate: Boolean,
    ) : SearchUseCase {
        private val logger = Logger("NewTabSearchUseCase")

        override fun invoke(
            searchTerms: String,
            searchEngine: SearchEngine?,
            parentSessionId: String?,
        ) {
            invoke(
                searchTerms,
                source = SessionState.Source.Internal.None,
                selected = true,
                private = isPrivate,
                searchEngine = searchEngine,
                parentSessionId = parentSessionId,
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
         * @param parentSessionId optional parent session to attach this new search session to
         */
        @Suppress("LongParameterList")
        operator fun invoke(
            searchTerms: String,
            source: SessionState.Source,
            selected: Boolean = true,
            private: Boolean = false,
            searchEngine: SearchEngine? = null,
            parentSessionId: String? = null,
        ) {
            val searchUrl = searchEngine?.let {
                searchEngine.buildSearchUrl(searchTerms)
            } ?: store.state.search.selectedOrDefaultSearchEngine?.buildSearchUrl(searchTerms)

            if (searchUrl == null) {
                logger.warn("No default search engine available to perform search")
                return
            }

            val id = tabsUseCases.addTab(
                searchUrl,
                parentId = parentSessionId,
                source = source,
                selectTab = selected,
                private = private,
            )

            store.dispatch(ContentAction.UpdateSearchTermsAction(id, searchTerms))
        }
    }

    /**
     * Adds a new search engine to the list of search engines the user can use for searches.
     */
    class AddNewSearchEngineUseCase(
        private val store: BrowserStore,
    ) {
        /**
         * Adds the given [searchEngine] to the list of search engines the user can use for searches.
         */
        operator fun invoke(
            searchEngine: SearchEngine,
        ) {
            when (searchEngine.type) {
                SearchEngine.Type.BUNDLED -> store.dispatch(
                    SearchAction.ShowSearchEngineAction(searchEngine.id),
                )

                SearchEngine.Type.BUNDLED_ADDITIONAL -> store.dispatch(
                    SearchAction.AddAdditionalSearchEngineAction(searchEngine.id),
                )

                SearchEngine.Type.CUSTOM -> store.dispatch(
                    SearchAction.UpdateCustomSearchEngineAction(searchEngine),
                )

                SearchEngine.Type.APPLICATION -> { /* Do nothing */ }
            }
        }
    }

    /**
     * Removes a search engine from the list of search engines the user can use for searches.
     */
    class RemoveExistingSearchEngineUseCase(
        private val store: BrowserStore,
    ) {
        /**
         * Removes the given [searchEngine] from the list of search engines the user can use for
         * searches.
         */
        operator fun invoke(
            searchEngine: SearchEngine,
        ) {
            when (searchEngine.type) {
                SearchEngine.Type.BUNDLED -> store.dispatch(
                    SearchAction.HideSearchEngineAction(searchEngine.id),
                )

                SearchEngine.Type.BUNDLED_ADDITIONAL -> store.dispatch(
                    SearchAction.RemoveAdditionalSearchEngineAction(searchEngine.id),
                )

                SearchEngine.Type.CUSTOM -> store.dispatch(
                    SearchAction.RemoveCustomSearchEngineAction(searchEngine.id),
                )

                SearchEngine.Type.APPLICATION -> { /* Do nothing */ }
            }
        }
    }

    /**
     * Marks a search engine as "selected" by the user to be the default search engine to perform
     * searches with.
     */
    class SelectSearchEngineUseCase(
        private val store: BrowserStore,
    ) {
        /**
         * Marks the given [searchEngine] as "selected" by the user to be the default search engine
         * to perform searches with.
         */
        operator fun invoke(
            searchEngine: SearchEngine,
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
                SearchAction.SelectSearchEngineAction(searchEngine.id, name),
            )
        }
    }

    val defaultSearch: DefaultSearchUseCase by lazy {
        DefaultSearchUseCase(store, tabsUseCases, sessionUseCases)
    }

    val newTabSearch: NewTabSearchUseCase by lazy {
        NewTabSearchUseCase(store, tabsUseCases, false)
    }

    val newPrivateTabSearch: NewTabSearchUseCase by lazy {
        NewTabSearchUseCase(store, tabsUseCases, true)
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
