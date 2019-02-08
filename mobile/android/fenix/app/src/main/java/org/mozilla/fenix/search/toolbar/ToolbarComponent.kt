/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.ViewGroup
import androidx.core.content.ContextCompat
import kotlinx.android.synthetic.main.component_search.*
import mozilla.components.browser.toolbar.BrowserToolbar
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

class ToolbarComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: SearchState = SearchState("", false)
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
        applyTheme()
    }

    fun getView(): BrowserToolbar = uiView.toolbar

    private fun applyTheme() {
        getView().textColor = ContextCompat.getColor(container.context,
            DefaultThemeManager.resolveAttribute(R.attr.awesomeBarTitleTextColor, container.context))
        getView().hintColor = ContextCompat.getColor(container.context,
            DefaultThemeManager.resolveAttribute(R.attr.awesomeBarDescriptionTextColor, container.context))
    }
}

data class SearchState(val query: String, val isEditing: Boolean) : ViewState

sealed class SearchAction : Action {
    data class UrlCommitted(val url: String) : SearchAction()
    data class TextChanged(val query: String) : SearchAction()
    object ToolbarTapped : SearchAction()
    data class ToolbarMenuItemTapped(val item: ToolbarMenu.Item) : SearchAction()
}

sealed class SearchChange : Change {
    data class QueryChanged(val query: String) : SearchChange()
}
