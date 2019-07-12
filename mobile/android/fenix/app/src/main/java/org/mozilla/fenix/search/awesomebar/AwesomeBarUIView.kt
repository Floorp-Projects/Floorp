/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import android.graphics.PorterDuff.Mode.SRC_IN
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.browser.search.SearchEngine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.utils.Settings

class AwesomeBarUIView(
    private val container: ViewGroup,
    actionEmitter: Observer<AwesomeBarAction>,
    changesObservable: Observable<AwesomeBarChange>
) :
    UIView<AwesomeBarState, AwesomeBarAction, AwesomeBarChange>(
        container,
        actionEmitter,
        changesObservable
    ) {
    override val view: BrowserAwesomeBar = LayoutInflater.from(container.context)
        .inflate(R.layout.component_awesomebar, container, true)
        .findViewById(R.id.awesomeBar)

    var state: AwesomeBarState? = null
        private set

    private var clipboardSuggestionProvider: ClipboardSuggestionProvider? = null
    private var sessionProvider: SessionSuggestionProvider? = null
    private var historyStorageProvider: HistoryStorageSuggestionProvider? = null
    private var shortcutsEnginePickerProvider: ShortcutsSuggestionProvider? = null
    private var bookmarksStorageSuggestionProvider: BookmarksStorageSuggestionProvider? = null

    private val searchSuggestionProvider: SearchSuggestionProvider?
        get() = searchSuggestionFromShortcutProvider ?: defaultSearchSuggestionProvider!!

    private var defaultSearchSuggestionProvider: SearchSuggestionProvider? = null
    private var searchSuggestionFromShortcutProvider: SearchSuggestionProvider? = null

    private val shortcutEngineManager by lazy {
        ShortcutEngineManager(
            this,
            actionEmitter,
            ::setShortcutEngine,
            ::showSuggestionProviders,
            ::showSearchSuggestionProvider
        )
    }

    private val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
        override fun invoke(url: String, flags: EngineSession.LoadUrlFlags) {
            actionEmitter.onNext(AwesomeBarAction.URLTapped(url))
        }
    }

    private val searchUseCase = object : SearchUseCases.SearchUseCase {
        override fun invoke(searchTerms: String, searchEngine: SearchEngine?) {
            actionEmitter.onNext(AwesomeBarAction.SearchTermsTapped(searchTerms, searchEngine))
        }
    }

    private val shortcutSearchUseCase = object : SearchUseCases.SearchUseCase {
        override fun invoke(searchTerms: String, searchEngine: SearchEngine?) {
            actionEmitter.onNext(
                AwesomeBarAction.SearchTermsTapped(
                    searchTerms,
                    state?.suggestionEngine
                )
            )
        }
    }

    init {
        with(container.context) {
            val primaryTextColor = ContextCompat.getColor(
                this,
                ThemeManager.resolveAttribute(R.attr.primaryText, this)
            )

            val draw = getDrawable(R.drawable.ic_link)
            draw?.setColorFilter(primaryTextColor, SRC_IN)
            clipboardSuggestionProvider = ClipboardSuggestionProvider(
                this,
                loadUrlUseCase,
                draw!!.toBitmap(),
                getString(R.string.awesomebar_clipboard_title)
            )

            sessionProvider =
                SessionSuggestionProvider(
                    components.core.sessionManager,
                    components.useCases.tabsUseCases.selectTab,
                    components.core.icons
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

            if (Settings.getInstance(container.context).showSearchSuggestions) {
                val searchDrawable = getDrawable(R.drawable.ic_search)
                searchDrawable?.setColorFilter(primaryTextColor, SRC_IN)
                defaultSearchSuggestionProvider =
                    SearchSuggestionProvider(
                        searchEngine = components.search.searchEngineManager.getDefaultSearchEngine(
                            this
                        ),
                        searchUseCase = searchUseCase,
                        fetchClient = components.core.client,
                        mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                        limit = 3,
                        icon = searchDrawable?.toBitmap()
                    )
            }

            shortcutsEnginePickerProvider =
                ShortcutsSuggestionProvider(
                    components.search.searchEngineManager,
                    this,
                    shortcutEngineManager::selectShortcutEngine,
                    shortcutEngineManager::selectShortcutEngineSettings
                )

            shortcutEngineManager.shortcutsEnginePickerProvider = shortcutsEnginePickerProvider

            val listener = object : RecyclerView.OnFlingListener() {
                override fun onFling(velocityX: Int, velocityY: Int): Boolean {
                    view.hideKeyboard()
                    return false
                }
            }

            view.onFlingListener = listener
        }
    }

    private fun showSuggestionProviders() {
        if (Settings.getInstance(container.context).showSearchSuggestions) {
            view.addProviders(searchSuggestionProvider!!)
        }

        if (Settings.getInstance(container.context).shouldShowVisitedSitesBookmarks) {
            view.addProviders(bookmarksStorageSuggestionProvider!!)
            view.addProviders(historyStorageProvider!!)
        }

        view.addProviders(
            clipboardSuggestionProvider!!,
            sessionProvider!!
        )
    }

    private fun showSearchSuggestionProvider() {
        if (Settings.getInstance(container.context).showSearchSuggestions) {
            view.addProviders(searchSuggestionProvider!!)
        }
    }

    private fun setShortcutEngine(engine: SearchEngine) {
        with(container.context) {
            val draw = getDrawable(R.drawable.ic_search)
            draw?.setColorFilter(
                ContextCompat.getColor(
                    this,
                    ThemeManager.resolveAttribute(R.attr.primaryText, this)
                ), SRC_IN
            )

            searchSuggestionFromShortcutProvider =
                SearchSuggestionProvider(
                    components.search.searchEngineManager.getDefaultSearchEngine(this, engine.name),
                    shortcutSearchUseCase,
                    components.core.client,
                    mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                    icon = draw?.toBitmap()
                )
        }
    }

    override fun updateView() = Consumer<AwesomeBarState> {
        shortcutEngineManager.updateSelectedEngineIfNecessary(it)
        shortcutEngineManager.updateEnginePickerVisibilityIfNecessary(it)

        view.onInputChanged(it.query)
        state = it
    }
}
