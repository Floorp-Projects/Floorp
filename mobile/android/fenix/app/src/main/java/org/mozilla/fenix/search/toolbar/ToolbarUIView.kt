/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import io.reactivex.functions.Consumer
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.ktx.android.content.res.pxToDp
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIView

class ToolbarUIView(container: ViewGroup, bus: ActionBusFactory) :
    UIView<SearchState>(container, bus) {

    val toolbarIntegration: ToolbarIntegration

    override val view: BrowserToolbar = LayoutInflater.from(container.context)
        .inflate(R.layout.component_search, container, true)
        .findViewById(R.id.toolbar)

    private val urlBackground = LayoutInflater.from(container.context)
        .inflate(R.layout.layout_url_backround, container, false)

    init {
        view.apply {
            onUrlClicked = { false }
            setOnUrlCommitListener { bus.emit(SearchAction::class.java, SearchAction.UrlCommitted(it)) }

            browserActionMargin = resources.pxToDp(browserActionMarginDp)
            urlBoxView = urlBackground
            urlBoxMargin = this.resources.pxToDp(urlBoxMarginDp)

            textColor = ContextCompat.getColor(context, R.color.searchText)
            textSize = toolbarTextSizeSp
            hint = context.getString(R.string.search_hint)
            hintColor = ContextCompat.getColor(context, R.color.searchText)

            setOnEditListener(object : mozilla.components.concept.toolbar.Toolbar.OnEditListener {
                override fun onTextChanged(text: String) {
                    bus.emit(SearchChange::class.java, SearchChange.QueryChanged(text))
                }

                override fun onStopEditing() {
                    bus.emit(SearchAction::class.java, SearchAction.UrlCommitted("foo"))
                }
            })
        }

        with(view.context) {
            toolbarIntegration = ToolbarIntegration(
                this,
                view,
                ShippedDomainsProvider().also { it.initialize(this) },
                components.core.historyStorage
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
