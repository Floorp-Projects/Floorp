package org.mozilla.fenix.search.awesomebar

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.LayoutInflater
import android.view.ViewGroup
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
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.UIView

class AwesomeBarUIView(
    container: ViewGroup,
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

    init {
        val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
            override fun invoke(url: String) {
                actionEmitter.onNext(AwesomeBarAction.URLTapped(url))
            }
        }

        val searchUseCase = object : SearchUseCases.SearchUseCase {
            override fun invoke(searchTerms: String, searchEngine: SearchEngine?) {
                actionEmitter.onNext(AwesomeBarAction.SearchTermsTapped(searchTerms, searchEngine))
            }
        }

        with(container.context) {
            view.addProviders(ClipboardSuggestionProvider(
                this,
                loadUrlUseCase,
                getDrawable(R.drawable.ic_link).toBitmap(),
                getString(R.string.awesomebar_clipboard_title)
                )
            )

            view.addProviders(
                SessionSuggestionProvider(
                    components.core.sessionManager,
                    components.useCases.tabsUseCases.selectTab
                ),
                HistoryStorageSuggestionProvider(
                    components.core.historyStorage,
                    loadUrlUseCase
                ),
                SearchSuggestionProvider(
                    components.search.searchEngineManager.getDefaultSearchEngine(this),
                    searchUseCase,
                    components.core.client,
                    SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS
                )
            )
        }
    }

    override fun updateView() = Consumer<AwesomeBarState> {
        view.onInputChanged(it.query)
    }
}
