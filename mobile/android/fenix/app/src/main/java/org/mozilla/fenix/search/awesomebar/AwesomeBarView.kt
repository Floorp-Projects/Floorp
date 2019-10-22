package org.mozilla.fenix.search.awesomebar

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.graphics.PorterDuff.Mode.SRC_IN
import android.graphics.PorterDuffColorFilter
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.graphics.drawable.toBitmap
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_search.*
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
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getColorFromAttr
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
     * Called whenever the Shortcuts button is clicked
     */
    fun onSearchShortcutsButtonClicked()
}

/**
 * View that contains and configures the BrowserAwesomeBar
 */
class AwesomeBarView(
    private val container: ViewGroup,
    val interactor: AwesomeBarInteractor
) : LayoutContainer {
    val view: BrowserAwesomeBar = LayoutInflater.from(container.context)
        .inflate(R.layout.component_awesomebar, container, true)
        .findViewById(R.id.awesomeBar)

    override val containerView: View?
        get() = container

    private val sessionProvider: SessionSuggestionProvider
    private val historyStorageProvider: HistoryStorageSuggestionProvider
    private val shortcutsEnginePickerProvider: ShortcutsSuggestionProvider
    private val bookmarksStorageSuggestionProvider: BookmarksStorageSuggestionProvider
    private val defaultSearchSuggestionProvider: SearchSuggestionProvider
    private val searchSuggestionProviderMap: MutableMap<SearchEngine, SearchSuggestionProvider>
    private var providersInUse = mutableSetOf<AwesomeBar.SuggestionProvider>()

    private val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
        override fun invoke(url: String, flags: EngineSession.LoadUrlFlags) {
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
    }

    init {
        view.itemAnimator = null

        with(container.context) {
            val primaryTextColor = getColorFromAttr(R.attr.primaryText)

            val draw = getDrawable(R.drawable.ic_link)!!
            draw.setColorFilter(primaryTextColor, SRC_IN)

            sessionProvider =
                SessionSuggestionProvider(
                    components.core.sessionManager,
                    selectTabUseCase,
                    components.core.icons,
                    excludeSelectedSession = true
                )

            historyStorageProvider =
                HistoryStorageSuggestionProvider(
                    components.core.historyStorage,
                    loadUrlUseCase,
                    components.core.icons
                )

            bookmarksStorageSuggestionProvider =
                BookmarksStorageSuggestionProvider(
                    components.core.bookmarksStorage,
                    loadUrlUseCase,
                    components.core.icons
                )

            val searchDrawable = getDrawable(R.drawable.ic_search)!!
            searchDrawable.setColorFilter(primaryTextColor, SRC_IN)

            defaultSearchSuggestionProvider =
                SearchSuggestionProvider(
                    context = this,
                    searchEngineManager = components.search.searchEngineManager,
                    searchUseCase = searchUseCase,
                    fetchClient = components.core.client,
                    mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                    limit = 3,
                    icon = searchDrawable.toBitmap()
                )

            shortcutsEnginePickerProvider =
                ShortcutsSuggestionProvider(
                    components.search.searchEngineManager,
                    this,
                    interactor::onSearchShortcutEngineSelected,
                    interactor::onClickSearchEngineSettings
                )
        }

        searchSuggestionProviderMap = HashMap()
        searchShortcutsButton.setOnClickListener {
            interactor.onSearchShortcutsButtonClicked()
        }
    }

    fun update(state: SearchFragmentState) {
        updateSearchShortcutsIcon(state)

        // Do not make suggestions based on user's current URL
        if (state.query == state.session?.url) {
            return
        }

        updateSuggestionProvidersVisibility(state)

        view.onInputChanged(state.query)
    }

    private fun updateSearchShortcutsIcon(searchState: SearchFragmentState) {
        with(container.context) {
            val showShortcuts = searchState.showSearchShortcuts
            searchShortcutsButton?.isChecked = showShortcuts

            val color = if (showShortcuts) R.attr.contrastText else R.attr.primaryText

            searchShortcutsButton.compoundDrawables[0]?.setTint(getColorFromAttr(color))
        }
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
        if (!searchSuggestionProviderMap.containsKey(engine)) {
            with(container.context) {
                val draw = getDrawable(R.drawable.ic_search)
                draw?.colorFilter =
                    PorterDuffColorFilter(getColorFromAttr(R.attr.primaryText), SRC_IN)

                searchSuggestionProviderMap.put(
                    engine, SearchSuggestionProvider(
                        components.search.searchEngineManager.getDefaultSearchEngine(
                            this,
                            engine.name
                        ),
                        shortcutSearchUseCase,
                        components.core.client,
                        limit = 3,
                        mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                        icon = draw?.toBitmap()
                    )
                )
            }
        }
        return searchSuggestionProviderMap[engine]
    }
}
