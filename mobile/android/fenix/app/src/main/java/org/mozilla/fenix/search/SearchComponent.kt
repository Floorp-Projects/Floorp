/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.annotation.SuppressLint
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_browser.*
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

class SearchComponent(
    private val container: ViewGroup,
    override val bus: ActionBusFactory,
    private val onEditComplete: (View) -> Unit,
    override var initialState: SearchState = SearchState("")
) :
    UIComponent<SearchState, SearchAction, SearchChange>(bus) {

    override val reducer: Reducer<SearchState, SearchChange> = { state, change ->
        when (change) {
            is SearchChange.Changed -> state // TODO handle state changes here
        }
    }

    override fun initView() = SearchUIView(container, bus)
    init {
        setup()
    }

    fun getView(): BrowserToolbar = uiView.toolbar
    fun editMode() = getView().editMode()

    @SuppressLint("CheckResult")
    fun setup(): SearchComponent {
        render(reducer)
        getUserInteractionEvents<SearchAction>()
            .subscribe {
                Logger("SearchComponent").debug(it.toString())
                when (it) {
                    is SearchAction.EditComplete -> {
                        onEditComplete.invoke(getView())
                    }
                    else -> {}
                }
            }
        return this
    }
}

data class SearchState(val term: String) : ViewState
sealed class SearchAction : Action {
    object UrlClicked : SearchAction()
    object EditComplete : SearchAction()
}

sealed class SearchChange : Change {
    object Changed : SearchChange()
}
