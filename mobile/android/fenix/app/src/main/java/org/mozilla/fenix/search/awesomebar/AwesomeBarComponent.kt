package org.mozilla.fenix.search.awesomebar
/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.ViewGroup
import org.mozilla.fenix.mvi.*

data class AwesomeBarState(val query: String) : ViewState {
    fun updateQuery(query: String) = AwesomeBarState(query)
}

sealed class AwesomeBarAction: Action {
    object ItemSelected: AwesomeBarAction()
}

sealed class AwesomeBarChange : Change {
    data class UpdateQuery(val query: String): AwesomeBarChange()
}

class AwesomeBarComponent(
    private val container: ViewGroup,
    override val bus: ActionBusFactory,
    override var initialState: AwesomeBarState = AwesomeBarState("")
) : UIComponent<AwesomeBarState, AwesomeBarAction, AwesomeBarChange>(bus) {
    override val reducer: Reducer<AwesomeBarState, AwesomeBarChange> = { state, change ->
        when (change) {
            is AwesomeBarChange.UpdateQuery -> state.updateQuery(change.query)
        }
    }

    override fun initView() = AwesomeBarUIView(container, bus)

    init {
        render(reducer)
    }
}
