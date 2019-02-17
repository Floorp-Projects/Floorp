/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.tabs

import android.view.ViewGroup
import mozilla.components.browser.session.Session
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

class TabsComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    private val isPrivate: Boolean,
    override var initialState: TabsState = TabsState(listOf())
) :
    UIComponent<TabsState, TabsAction, TabsChange>(
        bus.getManagedEmitter(TabsAction::class.java),
        bus.getSafeManagedObservable(TabsChange::class.java)
    ) {

    override val reducer: (TabsState, TabsChange) -> TabsState = { state, change ->
        when (change) {
            is TabsChange.Changed -> state.copy(sessions = change.sessions)
        }
    }

    override fun initView() = TabsUIView(container, actionEmitter, isPrivate, changesObservable)

    init {
        render(reducer)
    }
}

data class TabsState(val sessions: List<SessionViewState>) : ViewState
data class SessionViewState(val id: String, val url: String, val selected: Boolean)

fun Session.toSessionViewState(selected: Boolean): SessionViewState {
    return SessionViewState(this.id, this.url, selected)
}

sealed class TabsAction : Action {
    data class Select(val sessionId: String) : TabsAction()
    data class Close(val sessionId: String) : TabsAction()
}

sealed class TabsChange : Change {
    data class Changed(val sessions: List<SessionViewState>) : TabsChange()
}
