/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.ViewGroup
import android.widget.ImageView
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelProviders
import io.reactivex.Observable
import kotlinx.android.synthetic.main.component_search.*
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.toolbar.BrowserToolbar
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModel
import org.mozilla.fenix.mvi.ViewState

class ToolbarComponent(
    private val container: ViewGroup,
    owner: Fragment,
    bus: ActionBusFactory,
    private val sessionId: String?,
    private val isPrivate: Boolean,
    override var initialState: SearchState = SearchState("", "", false),
    private val engineIconView: ImageView? = null
) :
    UIComponent<SearchState, SearchAction, SearchChange>(
        owner,
        bus.getManagedEmitter(SearchAction::class.java),
        bus.getSafeManagedObservable(SearchChange::class.java)
    ) {

    fun getView(): BrowserToolbar = uiView.toolbar

    override fun initView() = ToolbarUIView(
        sessionId,
        isPrivate,
        container,
        actionEmitter,
        changesObservable,
        engineIconView
    )

    override fun render(): Observable<SearchState> =
        ViewModelProviders.of(owner, ToolbarViewModel.Factory(initialState, changesObservable))
            .get(ToolbarViewModel::class.java).render(changesObservable, uiView)

    init {
        render()
        applyTheme()
    }

    private fun applyTheme() {
        getView().suggestionBackgroundColor = ContextCompat.getColor(
            container.context,
            R.color.suggestion_highlight_color
        )
        getView().textColor = ContextCompat.getColor(
            container.context,
            DefaultThemeManager.resolveAttribute(R.attr.primaryText, container.context)
        )
        getView().hintColor = ContextCompat.getColor(
            container.context,
            DefaultThemeManager.resolveAttribute(R.attr.secondaryText, container.context)
        )
    }
}

data class SearchState(
    val query: String,
    val searchTerm: String,
    val isEditing: Boolean,
    val engine: SearchEngine? = null,
    val focused: Boolean = isEditing
) : ViewState

sealed class SearchAction : Action {
    data class UrlCommitted(val url: String, val session: String?, val engine: SearchEngine? = null) : SearchAction()
    data class TextChanged(val query: String) : SearchAction()
    object ToolbarClicked : SearchAction()
    object ToolbarLongClicked : SearchAction()
    data class ToolbarMenuItemTapped(val item: ToolbarMenu.Item) : SearchAction()
    object EditingCanceled : SearchAction()
}

sealed class SearchChange : Change {
    object ToolbarRequestedFocus : SearchChange()
    object ToolbarClearedFocus : SearchChange()
    data class SearchShortcutEngineSelected(val engine: SearchEngine) : SearchChange()
}

class ToolbarViewModel(initialState: SearchState) :
    UIComponentViewModel<SearchState, SearchAction, SearchChange>(initialState, reducer) {

    class Factory(
        private val initialState: SearchState,
        val changesObservable: Observable<SearchChange>
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel?> create(modelClass: Class<T>): T =
            ToolbarViewModel(initialState) as T
    }

    companion object {
        val reducer: Reducer<SearchState, SearchChange> = { state, change ->
            when (change) {
                is SearchChange.ToolbarClearedFocus -> state.copy(focused = false)
                is SearchChange.ToolbarRequestedFocus -> state.copy(focused = true)
                is SearchChange.SearchShortcutEngineSelected ->
                    state.copy(engine = change.engine)
            }
        }
    }
}
