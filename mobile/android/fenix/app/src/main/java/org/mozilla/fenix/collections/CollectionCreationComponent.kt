package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.ViewGroup
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

data class Tab(
    val sessionId: String,
    val url: String,
    val hostname: String,
    val title: String
)

data class CollectionCreationState(val tabs: List<Tab> = listOf()) : ViewState

sealed class CollectionCreationChange : Change {
    data class TabListChange(val tabs: List<Tab>) : CollectionCreationChange()
}

sealed class CollectionCreationAction : Action {
    object Close : CollectionCreationAction()
}

class CollectionCreationComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: CollectionCreationState = CollectionCreationState()
) : UIComponent<CollectionCreationState, CollectionCreationAction, CollectionCreationChange>(
    bus.getManagedEmitter(CollectionCreationAction::class.java),
    bus.getSafeManagedObservable(CollectionCreationChange::class.java)
) {
    override val reducer: Reducer<CollectionCreationState, CollectionCreationChange> = { state, change ->
        when (change) {
            is CollectionCreationChange.TabListChange -> state.copy(tabs = change.tabs)
        }
    }

    override fun initView() = CollectionCreationUIView(container, actionEmitter, changesObservable)

    init {
        render(reducer)
    }
}