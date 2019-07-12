/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.view.ViewGroup
import org.mozilla.fenix.mvi.ViewState
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.test.Mockable

data class ExceptionsItem(val url: String)

@Mockable
class ExceptionsComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    viewModelProvider: UIComponentViewModelProvider<ExceptionsState, ExceptionsChange>
) :
    UIComponent<ExceptionsState, ExceptionsAction, ExceptionsChange>(
        bus.getManagedEmitter(ExceptionsAction::class.java),
        bus.getSafeManagedObservable(ExceptionsChange::class.java),
        viewModelProvider
    ) {

    override fun initView() = ExceptionsUIView(container, actionEmitter, changesObservable)

    init {
        bind()
    }
}

data class ExceptionsState(val items: List<ExceptionsItem>) : ViewState

sealed class ExceptionsAction : Action {
    sealed class Delete : ExceptionsAction() {
        object All : Delete()
        data class One(val item: ExceptionsItem) : Delete()
    }
}

sealed class ExceptionsChange : Change {
    data class Change(val list: List<ExceptionsItem>) : ExceptionsChange()
}

class ExceptionsViewModel(
    initialState: ExceptionsState
) : UIComponentViewModelBase<ExceptionsState, ExceptionsChange>(initialState, reducer) {
    companion object {
        val reducer: (ExceptionsState, ExceptionsChange) -> ExceptionsState = { state, change ->
            when (change) {
                is ExceptionsChange.Change -> state.copy(items = change.list)
            }
        }
    }
}
