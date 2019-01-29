/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.fragment_search.view.*
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.awesomebar.AwesomeBarFeature
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.support.ktx.android.content.res.pxToDp
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIView

class SearchUIView(container: ViewGroup, bus: ActionBusFactory) :
    UIView<SearchState>(container, bus) {

    override val view: BrowserToolbar = LayoutInflater.from(container.context)
        .inflate(R.layout.component_search, container, true)
        .findViewById(R.id.toolbar)

    private val urlBackground = LayoutInflater.from(container.context)
        .inflate(R.layout.layout_url_backround, container, false)

    init {
        view.apply {
            onUrlClicked = {
                bus.emit(SearchAction::class.java, SearchAction.UrlClicked)
                false
            }

            browserActionMargin = resources.pxToDp(browserActionMarginDp)
            urlBoxView = urlBackground
            urlBoxMargin = this.resources.pxToDp(urlBoxMarginDp)

            textColor = ContextCompat.getColor(context, R.color.searchText)
            textSize = toolbarTextSizeSp
            hint = context.getString(R.string.search_hint)
            hintColor = ContextCompat.getColor(context, R.color.searchText)
        }

        with(container.context) {
            AwesomeBarFeature(container.rootView.awesomeBar, view, null,
                onEditComplete = { bus.emit(SearchAction::class.java, SearchAction.EditComplete) })
                .addClipboardProvider(this, components.useCases.sessionUseCases.loadUrl)
                .addSearchProvider(
                    components.search.searchEngineManager.getDefaultSearchEngine(this),
                    components.useCases.searchUseCases.defaultSearch,
                    SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS
                )
                .addSessionProvider(
                    components.core.sessionManager,
                    components.useCases.tabsUseCases.selectTab
                )
        }
    }

    override fun updateView() = Consumer<SearchState> {
    }

    companion object {
        const val toolbarTextSizeSp = 14f
        const val browserActionMarginDp = 8
        const val urlBoxMarginDp = 8
    }
}
