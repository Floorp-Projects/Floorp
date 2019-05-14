package org.mozilla.fenix.search.awesomebar

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelProviders
import io.reactivex.Observable
import mozilla.components.browser.search.SearchEngine
import org.mozilla.fenix.ext.logDebug
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModel
import org.mozilla.fenix.mvi.ViewState

data class AwesomeBarState(
    val query: String,
    val showShortcutEnginePicker: Boolean,
    val suggestionEngine: SearchEngine? = null
) : ViewState

sealed class AwesomeBarAction : Action {
    data class URLTapped(val url: String) : AwesomeBarAction()
    data class SearchTermsTapped(val searchTerms: String, val engine: SearchEngine? = null) : AwesomeBarAction()
    data class SearchShortcutEngineSelected(val engine: SearchEngine) : AwesomeBarAction()
}

sealed class AwesomeBarChange : Change {
    data class SearchShortcutEngineSelected(val engine: SearchEngine) : AwesomeBarChange()
    data class SearchShortcutEnginePicker(val show: Boolean) : AwesomeBarChange()
    data class UpdateQuery(val query: String) : AwesomeBarChange()
}

class AwesomeBarComponent(
    private val container: ViewGroup,
    owner: Fragment,
    bus: ActionBusFactory,
    override var initialState: AwesomeBarState = AwesomeBarState("", false)
) : UIComponent<AwesomeBarState, AwesomeBarAction, AwesomeBarChange>(
    owner,
    bus.getManagedEmitter(AwesomeBarAction::class.java),
    bus.getSafeManagedObservable(AwesomeBarChange::class.java)
) {
    override fun initView() = AwesomeBarUIView(container, actionEmitter, changesObservable)

    override fun render(): Observable<AwesomeBarState> =
        ViewModelProviders.of(owner, AwesomeBarViewModel.Factory(initialState))
            .get(AwesomeBarViewModel::class.java).render(changesObservable, uiView)

    init {
        render()
    }
}

class AwesomeBarViewModel(initialState: AwesomeBarState) :
    UIComponentViewModel<AwesomeBarState, AwesomeBarAction, AwesomeBarChange>(
        initialState,
        reducer
    ) {

    class Factory(
        private val initialState: AwesomeBarState
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel?> create(modelClass: Class<T>): T =
            AwesomeBarViewModel(initialState) as T
    }

    companion object {
        val reducer: Reducer<AwesomeBarState, AwesomeBarChange> = { state, change ->
            logDebug("IN_REDUCER", change.toString())
            when (change) {
                is AwesomeBarChange.SearchShortcutEngineSelected ->
                    state.copy(suggestionEngine = change.engine, showShortcutEnginePicker = false)
                is AwesomeBarChange.SearchShortcutEnginePicker ->
                    state.copy(showShortcutEnginePicker = change.show)
                is AwesomeBarChange.UpdateQuery -> state.copy(query = change.query)
            }
        }
    }
}
