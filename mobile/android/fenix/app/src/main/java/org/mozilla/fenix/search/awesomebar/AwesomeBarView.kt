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
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
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
import org.mozilla.fenix.search.SearchState

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

    private val clipboardSuggestionProvider: ClipboardSuggestionProvider
    private val sessionProvider: SessionSuggestionProvider
    private val historyStorageProvider: HistoryStorageSuggestionProvider
    private val shortcutsEnginePickerProvider: ShortcutsSuggestionProvider
    private val bookmarksStorageSuggestionProvider: BookmarksStorageSuggestionProvider
    private val defaultSearchSuggestionProvider: SearchSuggestionProvider

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
        with(container.context) {
            val primaryTextColor = getColorFromAttr(R.attr.primaryText)

            val draw = getDrawable(R.drawable.ic_link)!!
            draw.setColorFilter(primaryTextColor, SRC_IN)

            clipboardSuggestionProvider = ClipboardSuggestionProvider(
                this,
                loadUrlUseCase,
                draw.toBitmap(),
                getString(R.string.awesomebar_clipboard_title)
            )

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
                    searchEngine = components.search.searchEngineManager.getDefaultSearchEngine(
                        this
                    ),
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
    }

    fun update(state: SearchState) {
        view.removeAllProviders()

        if (state.showShortcutEnginePicker) {
            view.addProviders(shortcutsEnginePickerProvider)
        } else {
            if (state.showSuggestions) {
                view.addProviders(when (state.searchEngineSource) {
                    is SearchEngineSource.Default -> defaultSearchSuggestionProvider
                    is SearchEngineSource.Shortcut -> createSuggestionProviderForEngine(
                        state.searchEngineSource.searchEngine
                    )
                })
            }

            if (state.showVisitedSitesBookmarks) {
                view.addProviders(bookmarksStorageSuggestionProvider, historyStorageProvider)
            }

            view.addProviders(clipboardSuggestionProvider)

            if ((container.context.asActivity() as? HomeActivity)?.browsingModeManager?.mode?.isPrivate == false) {
                view.addProviders(sessionProvider)
            }
        }

        view.onInputChanged(state.query)
    }

    private fun createSuggestionProviderForEngine(engine: SearchEngine): SearchSuggestionProvider {
        return with(container.context) {
            val draw = getDrawable(R.drawable.ic_search)
            draw?.colorFilter = PorterDuffColorFilter(getColorFromAttr(R.attr.primaryText), SRC_IN)

            SearchSuggestionProvider(
                components.search.searchEngineManager.getDefaultSearchEngine(this, engine.name),
                shortcutSearchUseCase,
                components.core.client,
                limit = 3,
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                icon = draw?.toBitmap()
            )
        }
    }
}
