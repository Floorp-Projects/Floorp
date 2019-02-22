/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.view.ViewGroup
import mozilla.components.browser.session.Session
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

data class ArchivedSession(val id: Long, private val savedAt: Long, val urls: List<String>)

class SessionsComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: SessionsState = SessionsState(emptyList())
) :
    UIComponent<SessionsState, SessionsAction, SessionsChange>(
        bus.getManagedEmitter(SessionsAction::class.java),
        bus.getSafeManagedObservable(SessionsChange::class.java)
    ) {

    override val reducer: (SessionsState, SessionsChange) -> SessionsState = { state, change ->
        when (change) {
            is SessionsChange.Changed -> state.copy(archivedSessions = change.archivedSessions) // copy state with changes here
        }
    }

    override fun initView() = SessionsUIView(container, actionEmitter, changesObservable)

    init {
        render(reducer)
    }
}

data class SessionsState(val archivedSessions: List<ArchivedSession>) : ViewState

sealed class SessionsAction : Action {
    data class Select(val archivedSession: ArchivedSession) : SessionsAction()
}

sealed class SessionsChange : Change {
    data class Changed(val archivedSessions: List<ArchivedSession>) : SessionsChange()
}
