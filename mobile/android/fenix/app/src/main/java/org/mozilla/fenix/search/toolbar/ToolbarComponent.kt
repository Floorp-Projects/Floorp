/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_browser.*
import mozilla.components.browser.toolbar.BrowserToolbar
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

class ToolbarComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: SearchState = SearchState("")
) :
    UIComponent<SearchState, SearchAction, SearchChange>(
        bus.getManagedEmitter(SearchAction::class.java),
        bus.getSafeManagedObservable(SearchChange::class.java)
    ) {

    override val reducer: Reducer<SearchState, SearchChange> = { state, change ->
        when (change) {
            is SearchChange.QueryChanged -> state.copy(query = change.query)
        }
    }

    override fun initView() = ToolbarUIView(container, actionEmitter, changesObservable)
    init {
        render(reducer)
    }

    fun getView(): BrowserToolbar = uiView.toolbar
    fun editMode() = getView().editMode()
}

data class SearchState(val query: String) : ViewState

sealed class SearchAction : Action {
    data class UrlCommitted(val url: String) : SearchAction()
    data class TextChanged(val query: String) : SearchAction()
}

sealed class SearchChange : Change {
    data class QueryChanged(val query: String) : SearchChange()
}
