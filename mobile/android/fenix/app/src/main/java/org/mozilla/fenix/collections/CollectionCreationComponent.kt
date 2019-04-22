package org.mozilla.fenix.collections

import android.view.ViewGroup
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

sealed class CollectionCreationState : ViewState {
    object Empty : CollectionCreationState()
}

sealed class CollectionCreationChange : Change {

}

sealed class CollectionCreationAction : Action {

}

class CollectionCreationComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: CollectionCreationState = CollectionCreationState.Empty
) : UIComponent<CollectionCreationState, CollectionCreationAction, CollectionCreationChange>(
    bus.getManagedEmitter(CollectionCreationAction::class.java),
    bus.getSafeManagedObservable(CollectionCreationChange::class.java)
) {
    override val reducer: Reducer<CollectionCreationState, CollectionCreationChange> = { state, change ->
        CollectionCreationState.Empty
    }

    override fun initView() = CollectionCreationUIView(container, actionEmitter, changesObservable)

    init {
        render(reducer)
    }
}