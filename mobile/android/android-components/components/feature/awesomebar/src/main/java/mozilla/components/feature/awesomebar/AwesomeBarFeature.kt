/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar

import android.content.Context
import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.drawable.Drawable
import android.view.View
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchActionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases

/**
 * Connects an [AwesomeBar] with a [Toolbar] and allows adding multiple [AwesomeBar.SuggestionProvider] implementations.
 */
@Suppress("LongParameterList")
class AwesomeBarFeature(
    private val awesomeBar: AwesomeBar,
    private val toolbar: Toolbar,
    private val engineView: EngineView? = null,
    private val icons: BrowserIcons? = null,
    private val indicatorIcon: Drawable? = null,
    onEditStart: (() -> Unit)? = null,
    onEditComplete: (() -> Unit)? = null,
) {
    init {
        toolbar.setOnEditListener(
            ToolbarEditListener(
                awesomeBar,
                onEditStart,
                onEditComplete,
                ::showAwesomeBar,
                ::hideAwesomeBar,
            ),
        )

        awesomeBar.setOnStopListener { toolbar.displayMode() }
        awesomeBar.setOnEditSuggestionListener(toolbar::setSearchTerms)
    }

    /**
     * Add a [AwesomeBar.SuggestionProvider] for "Open tabs" to the [AwesomeBar].
     */
    fun addSessionProvider(
        resources: Resources,
        store: BrowserStore,
        selectTabUseCase: TabsUseCases.SelectTabUseCase,
    ): AwesomeBarFeature {
        val provider = SessionSuggestionProvider(resources, store, selectTabUseCase, icons, indicatorIcon)
        awesomeBar.addProviders(provider)
        return this
    }

    /**
     * Adds a [AwesomeBar.SuggestionProvider] for search engine suggestions to the [AwesomeBar].
     *
     * @param searchEngine The search engine to request suggestions from.
     * @param searchUseCase The use case to invoke for searches.
     * @param fetchClient The HTTP client for requesting suggestions from the search engine.
     * @param limit The maximum number of suggestions that should be returned. It needs to be >= 1.
     * @param mode Whether to return a single search suggestion (with chips) or one suggestion per item.
     * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
     * highest scored search suggestion URL.
     * @param filterExactMatch If true filters out suggestions that exactly match the entered text.
     */
    @Suppress("LongParameterList")
    fun addSearchProvider(
        searchEngine: SearchEngine,
        searchUseCase: SearchUseCases.SearchUseCase,
        fetchClient: Client,
        limit: Int = 15,
        mode: SearchSuggestionProvider.Mode = SearchSuggestionProvider.Mode.SINGLE_SUGGESTION,
        engine: Engine? = null,
        filterExactMatch: Boolean = false,
    ): AwesomeBarFeature {
        awesomeBar.addProviders(
            SearchSuggestionProvider(
                searchEngine,
                searchUseCase,
                fetchClient,
                limit,
                mode,
                engine,
                filterExactMatch = filterExactMatch,
            ),
        )
        return this
    }

    /**
     * Adds a [AwesomeBar.SuggestionProvider] for search engine suggestions to the [AwesomeBar].
     * If the default search engine is to be used for fetching search engine suggestions then
     * this method is preferable over [addSearchProvider], as it will read the search engine from
     * the provided [BrowserStore].
     *
     * @param context the activity or application context, required to load search engines.
     * @param store The [BrowserStore] to lookup search engines from.
     * @param searchUseCase The use case to invoke for searches.
     * @param fetchClient The HTTP client for requesting suggestions from the search engine.
     * @param limit The maximum number of suggestions that should be returned. It needs to be >= 1.
     * @param mode Whether to return a single search suggestion (with chips) or one suggestion per item.
     * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
     * highest scored search suggestion URL.
     * @param filterExactMatch If true filters out suggestions that exactly match the entered text.
     */
    @Suppress("LongParameterList")
    fun addSearchProvider(
        context: Context,
        store: BrowserStore,
        searchUseCase: SearchUseCases.SearchUseCase,
        fetchClient: Client,
        limit: Int = 15,
        mode: SearchSuggestionProvider.Mode = SearchSuggestionProvider.Mode.SINGLE_SUGGESTION,
        engine: Engine? = null,
        filterExactMatch: Boolean = false,
    ): AwesomeBarFeature {
        awesomeBar.addProviders(
            SearchSuggestionProvider(
                context,
                store,
                searchUseCase,
                fetchClient,
                limit,
                mode,
                engine,
                filterExactMatch = filterExactMatch,
            ),
        )
        return this
    }

    /**
     * Adds an [AwesomeBar.SuggestionProvider] implementation that always returns a suggestion that
     * mirrors the entered text and invokes a search with the given [SearchEngine] if clicked.
     *
     * @param store The [BrowserStore] to read the default search engine from.
     * @param searchUseCase The use case to invoke for searches.
     * @param icon The image to display next to the result. If not specified, the engine icon is used.
     * @param showDescription whether or not to add the search engine name as description.
     */
    fun addSearchActionProvider(
        store: BrowserStore,
        searchUseCase: SearchUseCases.SearchUseCase,
        icon: Bitmap? = null,
        showDescription: Boolean = false,
    ): AwesomeBarFeature {
        awesomeBar.addProviders(
            SearchActionProvider(
                store,
                searchUseCase,
                icon,
                showDescription,
            ),
        )
        return this
    }

    /**
     * Add a [AwesomeBar.SuggestionProvider] for browsing history to the [AwesomeBar].
     *
     * @param historyStorage an instance of the [HistoryStorage] used to query matching history.
     * @param loadUrlUseCase the use case invoked to load the url when the user clicks on the suggestion.
     * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
     * highest scored suggestion URL.
     * @param maxNumberOfSuggestions optional parameter to specify the maximum number of returned suggestions.
     * Zero or a negative value here means the default number of history suggestions will be returned.
     */
    fun addHistoryProvider(
        historyStorage: HistoryStorage,
        loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
        engine: Engine? = null,
        maxNumberOfSuggestions: Int = -1,
    ): AwesomeBarFeature {
        awesomeBar.addProviders(
            if (maxNumberOfSuggestions <= 0) {
                HistoryStorageSuggestionProvider(historyStorage, loadUrlUseCase, icons, engine)
            } else {
                HistoryStorageSuggestionProvider(historyStorage, loadUrlUseCase, icons, engine, maxNumberOfSuggestions)
            },
        )
        return this
    }

    /**
     * Add a [AwesomeBar.SuggestionProvider] for clipboard items to the [AwesomeBar].
     *
     * @param context the activity or application context, required to look up the clipboard manager.
     * @param loadUrlUseCase the use case invoked to load the url when
     * the user clicks on the suggestion.
     * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
     * highest scored suggestion URL.
     */
    fun addClipboardProvider(
        context: Context,
        loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
        engine: Engine? = null,
    ): AwesomeBarFeature {
        awesomeBar.addProviders(ClipboardSuggestionProvider(context, loadUrlUseCase, engine = engine))
        return this
    }

    private fun showAwesomeBar() {
        engineView?.asView()?.visibility = View.GONE
        awesomeBar.asView().visibility = View.VISIBLE
    }

    private fun hideAwesomeBar() {
        awesomeBar.asView().visibility = View.GONE
        engineView?.asView()?.visibility = View.VISIBLE
    }
}

internal class ToolbarEditListener(
    private val awesomeBar: AwesomeBar,
    private val onEditStart: (() -> Unit)? = null,
    private val onEditComplete: (() -> Unit)? = null,
    private val showAwesomeBar: () -> Unit,
    private val hideAwesomeBar: () -> Unit,
) : Toolbar.OnEditListener {
    private var inputStarted = false

    override fun onTextChanged(text: String) {
        if (inputStarted) {
            awesomeBar.onInputChanged(text)
        }
    }

    override fun onStartEditing() {
        onEditStart?.invoke() ?: showAwesomeBar()
        awesomeBar.onInputStarted()
        inputStarted = true
    }

    override fun onStopEditing() {
        onEditComplete?.invoke() ?: hideAwesomeBar()
        awesomeBar.onInputCancelled()
        inputStarted = false
    }

    override fun onCancelEditing(): Boolean {
        return true
    }
}
