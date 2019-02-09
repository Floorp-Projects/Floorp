/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.library.history

import android.view.ViewGroup
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState
import java.net.URL

data class HistoryItem(val url: String) {
    val title: String
        get() = siteTitle()

    @SuppressWarnings("TooGenericExceptionCaught")
    private fun siteTitle(): String {
        return try {
            URL(url).host
        } catch (e: Exception) {
            url
        }
    }
}

class HistoryComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: HistoryState = HistoryState(emptyList())
) :
    UIComponent<HistoryState, HistoryAction, HistoryChange>(
        bus.getManagedEmitter(HistoryAction::class.java),
        bus.getSafeManagedObservable(HistoryChange::class.java)
    ) {

    override val reducer: (HistoryState, HistoryChange) -> HistoryState = { state, change ->
        when (change) {
            is HistoryChange.Change -> state.copy(items = change.list)
        }
    }

    override fun initView() = HistoryUIView(container, actionEmitter, changesObservable)

    init {
        render(reducer)
    }
}

data class HistoryState(val items: List<HistoryItem>) : ViewState

sealed class HistoryAction : Action {
    data class Select(val item: HistoryItem) : HistoryAction()
}

sealed class HistoryChange : Change {
    data class Change(val list: List<HistoryItem>) : HistoryChange()
}
