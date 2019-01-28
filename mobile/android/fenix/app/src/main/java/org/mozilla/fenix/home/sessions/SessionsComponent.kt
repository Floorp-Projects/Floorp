/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.annotation.SuppressLint
import android.view.ViewGroup
import mozilla.components.browser.session.Session
import org.mozilla.fenix.mvi.*

class SessionsComponent(private val container: ViewGroup, override val bus: ActionBusFactory) :
    UIComponent<SessionsState, SessionsAction, SessionsChange>(bus) {

    override var initialState: SessionsState = SessionsState(emptyList())

    override val reducer : (SessionsState, SessionsChange) -> SessionsState = { state, change ->
        when (change) {
            is SessionsChange.SessionsChanged -> state // copy state with changes here
        }
    }

    override fun initView() = SessionsUIView(container, bus)

    @SuppressLint("CheckResult")
    fun setup(): SessionsComponent {
        render(reducer)
        return this
    }
}

data class SessionsState(val sessions: List<Session>) : ViewState

sealed class SessionsAction : Action {
    object Select : SessionsAction()
}

sealed class SessionsChange : Change {
    object SessionsChanged : SessionsChange()
}
