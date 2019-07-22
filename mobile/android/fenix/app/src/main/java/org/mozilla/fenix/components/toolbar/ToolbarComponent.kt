/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.ViewGroup
import kotlinx.android.synthetic.main.component_search.*
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.mvi.ViewState

class ToolbarComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    private val sessionId: String?,
    private val isPrivate: Boolean,
    viewModelProvider: UIComponentViewModelProvider<SearchState, SearchChange>
) :
    UIComponent<SearchState, SearchAction, SearchChange>(
        bus.getManagedEmitter(SearchAction::class.java),
        bus.getSafeManagedObservable(SearchChange::class.java),
        viewModelProvider
    ) {

    override fun initView() = ToolbarUIView(
        sessionId,
        isPrivate,
        container,
        actionEmitter,
        changesObservable
    )

    init {
        bind()
    }

    fun setOnSiteSecurityClickedListener(listener: () -> Unit) {
        uiView.toolbar.setOnSiteSecurityClickedListener(listener)
    }
}

class SearchState : ViewState

sealed class SearchAction : Action {
    object ToolbarClicked : SearchAction()
    data class ToolbarMenuItemTapped(val item: ToolbarMenu.Item) : SearchAction()
}

sealed class SearchChange : Change

class ToolbarViewModel(initialState: SearchState) :
    UIComponentViewModelBase<SearchState, SearchChange>(initialState, reducer) {

    companion object {
        val reducer: Reducer<SearchState, SearchChange> = { state, _ -> state }
    }
}
