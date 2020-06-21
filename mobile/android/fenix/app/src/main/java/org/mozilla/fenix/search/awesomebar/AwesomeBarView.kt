/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.core.graphics.BlendModeColorFilterCompat.createBlendModeColorFilterCompat
import androidx.core.graphics.BlendModeCompat.SRC_IN
import androidx.core.graphics.drawable.toBitmap
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.SearchFragmentState

/**
 * Interface for the AwesomeBarView Interactor. This interface is implemented by objects that want
 * to respond to user interaction on the AwesomebarView
 */
interface AwesomeBarInteractor {

    /**
     * Called whenever a suggestion containing a URL is tapped
     * @param url the url the suggestion was providing
     */
    fun onUrlTapped(url: String)

    /**
     * Called whenever a search engine suggestion is tapped
     * @param searchTerms the query contained by the search suggestion
     */
    fun onSearchTermsTapped(searchTerms: String)

    /**
     * Called whenever a search engine shortcut is tapped
     * @param searchEngine the searchEngine that was selected
     */
    fun onSearchShortcutEngineSelected(searchEngine: SearchEngine)

    /**
     * Called whenever the "Search Engine Settings" item is tapped
     */
    fun onClickSearchEngineSettings()

    /**
     * Called whenever an existing session is selected from the sessionSuggestionProvider
     */
    fun onExistingSessionSelected(session: Session)

    /**
     * Called whenever an existing session is selected from the sessionSuggestionProvider
     */
    fun onExistingSessionSelected(tabId: String)

    /**
     * Called whenever the Shortcuts button is clicked
     */
    fun onSearchShortcutsButtonClicked()
}

/**
 * View that contains and configures the BrowserAwesomeBar
 */
class AwesomeBarView(
    private val container: ViewGroup,
    val interactor: AwesomeBarInteractor,
    val view: BrowserAwesomeBar
) {

    private val sessionProvider: SessionSuggestionProvider
    private val historyStorageProvider: HistoryStorageSuggestionProvider
    private val shortcutsEnginePickerProvider: ShortcutsSuggestionProvider
    private val bookmarksStorageSuggestionProvider: BookmarksStorageSuggestionProvider
    private val defaultSearchSuggestionProvider: SearchSuggestionProvider
    private val searchSuggestionProviderMap: MutableMap<SearchEngine, SearchSuggestionProvider>
    private var providersInUse = mutableSetOf<AwesomeBar.SuggestionProvider>()

    private val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
        override fun invoke(
            url: String,
            flags: EngineSession.LoadUrlFlags,
            additionalHeaders: Map<String, String>?
        ) {
            interactor.onUrlTapped(url)
        }
    }

    private val searchUseCase = object : SearchUseCases.SearchUseCase {
        override fun invoke(searchTerms: String, searchEngine: SearchEngine?) {
            interactor.onSearchTermsTapped(searchTerms)
        }
    }

    private val shortcutSearchUseCase = object : SearchUseCases.SearchUseCase {
        override fun invoke(searchTerms: String, searchEngine: SearchEngine?) {
            interactor.onSearchTermsTapped(searchTerms)
        }
    }

    private val selectTabUseCase = object : TabsUseCases.SelectTabUseCase {
        override fun invoke(session: Session) {
            interactor.onExistingSessionSelected(session)
        }

        override fun invoke(tabId: String) {
            interactor.onExistingSessionSelected(tabId)
        }
    }

    init {
        view.itemAnimator = null

        val context = container.context
        val components = context.components
        val primaryTextColor = context.getColorFromAttr(R.attr.primaryText)

        val draw = getDrawable(context, R.drawable.ic_link)!!
        draw.colorFilter = createBlendModeColorFilterCompat(primaryTextColor, SRC_IN)

        val engineForSpeculativeConnects = if (!isBrowsingModePrivate()) components.core.engine else null
        sessionProvider =
            SessionSuggestionProvider(
                context.resources,
                components.core.store,
                selectTabUseCase,
                components.core.icons,
                excludeSelectedSession = true
            )

        historyStorageProvider =
            HistoryStorageSuggestionProvider(
                components.core.historyStorage,
                loadUrlUseCase,
                components.core.icons,
                engineForSpeculativeConnects
            )

        bookmarksStorageSuggestionProvider =
            BookmarksStorageSuggestionProvider(
                components.core.bookmarksStorage,
                loadUrlUseCase,
                components.core.icons,
                engineForSpeculativeConnects
            )

        val searchDrawable = getDrawable(context, R.drawable.ic_search)!!
        searchDrawable.colorFilter = createBlendModeColorFilterCompat(primaryTextColor, SRC_IN)

        defaultSearchSuggestionProvider =
            SearchSuggestionProvider(
                context = context,
                searchEngineManager = components.search.searchEngineManager,
                searchUseCase = searchUseCase,
                fetchClient = components.core.client,
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                limit = 3,
                icon = searchDrawable.toBitmap(),
                showDescription = false,
                engine = engineForSpeculativeConnects
            )

        shortcutsEnginePickerProvider =
            ShortcutsSuggestionProvider(
                searchEngineProvider = components.search.provider,
                context = context,
                selectShortcutEngine = interactor::onSearchShortcutEngineSelected,
                selectShortcutEngineSettings = interactor::onClickSearchEngineSettings
            )

        searchSuggestionProviderMap = HashMap()
    }

    fun update(state: SearchFragmentState) {
        updateSuggestionProvidersVisibility(state)

        // Do not make suggestions based on user's current URL unless it's a search shortcut
        if (state.query == state.session?.url && !state.showSearchShortcuts) {
            return
        }

        view.onInputChanged(state.query)
    }

    private fun updateSuggestionProvidersVisibility(state: SearchFragmentState) {
        if (state.showSearchShortcuts) {
            handleDisplayShortcutsProviders()
            return
        }

        val providersToAdd = getProvidersToAdd(state)
        val providersToRemove = getProvidersToRemove(state)

        performProviderListChanges(providersToAdd, providersToRemove)
    }

    private fun performProviderListChanges(
        providersToAdd: MutableSet<AwesomeBar.SuggestionProvider>,
        providersToRemove: MutableSet<AwesomeBar.SuggestionProvider>
    ) {
        for (provider in providersToAdd) {
            if (providersInUse.find { it.id == provider.id } == null) {
                providersInUse.add(provider)
                view.addProviders(provider)
            }
        }

        for (provider in providersToRemove) {
            if (providersInUse.find { it.id == provider.id } != null) {
                providersInUse.remove(provider)
                view.removeProviders(provider)
            }
        }
    }

    private fun getProvidersToAdd(state: SearchFragmentState): MutableSet<AwesomeBar.SuggestionProvider> {
        val providersToAdd = mutableSetOf<AwesomeBar.SuggestionProvider>()

        if (state.showHistorySuggestions) {
            providersToAdd.add(historyStorageProvider)
        }

        if (state.showBookmarkSuggestions) {
            providersToAdd.add(bookmarksStorageSuggestionProvider)
        }

        if (state.showSearchSuggestions) {
            getSelectedSearchSuggestionProvider(state)?.let {
                providersToAdd.add(it)
            }
        }

        if (!isBrowsingModePrivate()) {
            providersToAdd.add(sessionProvider)
        }

        return providersToAdd
    }

    private fun getProvidersToRemove(state: SearchFragmentState): MutableSet<AwesomeBar.SuggestionProvider> {
        val providersToRemove = mutableSetOf<AwesomeBar.SuggestionProvider>()

        providersToRemove.add(shortcutsEnginePickerProvider)

        if (!state.showHistorySuggestions) {
            providersToRemove.add(historyStorageProvider)
        }

        if (!state.showBookmarkSuggestions) {
            providersToRemove.add(bookmarksStorageSuggestionProvider)
        }

        if (!state.showSearchSuggestions) {
            getSelectedSearchSuggestionProvider(state)?.let {
                providersToRemove.add(it)
            }
        }

        if (isBrowsingModePrivate()) {
            providersToRemove.add(sessionProvider)
        }

        return providersToRemove
    }

    private fun isBrowsingModePrivate(): Boolean {
        return (container.context.asActivity() as? HomeActivity)?.browsingModeManager?.mode?.isPrivate
            ?: false
    }

    private fun getSelectedSearchSuggestionProvider(state: SearchFragmentState): SearchSuggestionProvider? {
        return when (state.searchEngineSource) {
            is SearchEngineSource.Default -> defaultSearchSuggestionProvider
            is SearchEngineSource.Shortcut -> getSuggestionProviderForEngine(
                state.searchEngineSource.searchEngine
            )
        }
    }

    private fun handleDisplayShortcutsProviders() {
        view.removeAllProviders()
        providersInUse.clear()
        providersInUse.add(shortcutsEnginePickerProvider)
        view.addProviders(shortcutsEnginePickerProvider)
    }

    private fun getSuggestionProviderForEngine(engine: SearchEngine): SearchSuggestionProvider? {
        return searchSuggestionProviderMap.getOrPut(engine) {
            val context = container.context
            val components = context.components
            val primaryTextColor = context.getColorFromAttr(R.attr.primaryText)

            val draw = getDrawable(context, R.drawable.ic_search)
            draw?.colorFilter = createBlendModeColorFilterCompat(primaryTextColor, SRC_IN)

            val engineForSpeculativeConnects = if (!isBrowsingModePrivate()) components.core.engine else null

            SearchSuggestionProvider(
                components.search.provider.installedSearchEngines(context).list.find { it.name == engine.name }
                    ?: components.search.provider.getDefaultEngine(context),
                shortcutSearchUseCase,
                components.core.client,
                limit = 3,
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                icon = draw?.toBitmap(),
                engine = engineForSpeculativeConnects
            )
        }
    }
}
