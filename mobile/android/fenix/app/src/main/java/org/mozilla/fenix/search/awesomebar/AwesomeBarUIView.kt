package org.mozilla.fenix.search.awesomebar

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.graphics.PorterDuff
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import mozilla.components.browser.awesomebar.BrowserAwesomeBar
import mozilla.components.browser.search.SearchEngine
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.android.graphics.drawable.toBitmap
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
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
        override fun invoke(url: String) {
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
            clipboardSuggestionProvider = ClipboardSuggestionProvider(
                this,
                loadUrlUseCase,
                getDrawable(R.drawable.ic_link)!!.toBitmap(),
                getString(R.string.awesomebar_clipboard_title)
            )

            sessionProvider =
                SessionSuggestionProvider(
                    components.core.sessionManager,
                    components.useCases.tabsUseCases.selectTab,
                    components.utils.icons
                )

            historyStorageProvider =
                HistoryStorageSuggestionProvider(
                    components.core.historyStorage,
                    loadUrlUseCase,
                    components.utils.icons
                )

            if (Settings.getInstance(container.context).showSearchSuggestions()) {
                val draw = getDrawable(R.drawable.ic_search)
                draw?.setColorFilter(
                    ContextCompat.getColor(
                        this,
                        DefaultThemeManager.resolveAttribute(R.attr.searchShortcutsTextColor, this)
                    ), PorterDuff.Mode.SRC_IN
                )
                defaultSearchSuggestionProvider =
                    SearchSuggestionProvider(
                        searchEngine = components.search.searchEngineManager.getDefaultSearchEngine(
                            this
                        ),
                        searchUseCase = searchUseCase,
                        fetchClient = components.core.client,
                        mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                        limit = 3,
                        icon = draw?.toBitmap()
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
        }
    }

    private fun showSuggestionProviders() {
        if (Settings.getInstance(container.context).showSearchSuggestions()) {
            view.addProviders(searchSuggestionProvider!!)
        }

        view.addProviders(
            clipboardSuggestionProvider!!,
            historyStorageProvider!!,
            sessionProvider!!
        )
    }

    private fun showSearchSuggestionProvider() {
        view.addProviders(searchSuggestionProvider!!)
    }

    private fun setShortcutEngine(engine: SearchEngine) {
        with(container.context) {
            val draw = getDrawable(R.drawable.ic_search)
            draw?.setColorFilter(
                ContextCompat.getColor(
                    this,
                    DefaultThemeManager.resolveAttribute(R.attr.searchShortcutsTextColor, this)
                ), PorterDuff.Mode.SRC_IN
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
