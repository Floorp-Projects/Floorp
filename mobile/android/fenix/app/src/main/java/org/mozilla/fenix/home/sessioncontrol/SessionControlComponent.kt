/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.graphics.Bitmap
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.session.bundling.SessionBundle
import io.reactivex.Observer
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

class SessionControlComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: SessionControlState = SessionControlState(emptyList(), emptyList(), Mode.Normal)
) :
    UIComponent<SessionControlState, SessionControlAction, SessionControlChange>(
        bus.getManagedEmitter(SessionControlAction::class.java),
        bus.getSafeManagedObservable(SessionControlChange::class.java)
    ) {

    override val reducer: (SessionControlState, SessionControlChange) -> SessionControlState = { state, change ->
        when (change) {
            is SessionControlChange.TabsChange -> state.copy(tabs = change.tabs)
            is SessionControlChange.ArchivedSessionsChange ->
                state.copy(archivedSessions = change.archivedSessions)
            is SessionControlChange.ModeChange -> state.copy(mode = change.mode)
        }
    }

    override fun initView() = SessionControlUIView(container, actionEmitter, changesObservable)
    val view: RecyclerView
        get() = uiView.view as RecyclerView

    init {
        render(reducer)
    }
}

data class Tab(val sessionId: String, val url: String, val selected: Boolean, val thumbnail: Bitmap? = null)
data class ArchivedSession(val id: Long, val bundle: SessionBundle, val savedAt: Long, val urls: List<String>)
sealed class Mode {
    object Normal : Mode()
    object Private : Mode()
}

data class SessionControlState(
    val tabs: List<Tab>,
    val archivedSessions: List<ArchivedSession>,
    val mode: Mode
) : ViewState

sealed class ArchivedSessionAction : Action {
    data class Select(val session: ArchivedSession) : ArchivedSessionAction()
    data class Delete(val session: ArchivedSession) : ArchivedSessionAction()
    data class MenuTapped(val session: ArchivedSession) : ArchivedSessionAction()
    data class ShareTapped(val session: ArchivedSession) : ArchivedSessionAction()
}

sealed class TabAction : Action {
    object Archive : TabAction()
    object MenuTapped : TabAction()
    object Add : TabAction()
    data class CloseAll(val private: Boolean) : TabAction()
    data class Select(val sessionId: String) : TabAction()
    data class Close(val sessionId: String) : TabAction()
    object PrivateBrowsingLearnMore : TabAction()
}

sealed class SessionControlAction : Action {
    data class Tab(val action: TabAction) : SessionControlAction()
    data class Session(val action: ArchivedSessionAction) : SessionControlAction()
}

fun Observer<SessionControlAction>.onNext(tabAction: TabAction) {
    onNext(SessionControlAction.Tab(tabAction))
}

fun Observer<SessionControlAction>.onNext(archivedSessionAction: ArchivedSessionAction) {
    onNext(SessionControlAction.Session(archivedSessionAction))
}

sealed class SessionControlChange : Change {
    data class ArchivedSessionsChange(val archivedSessions: List<ArchivedSession>) : SessionControlChange()
    data class TabsChange(val tabs: List<Tab>) : SessionControlChange()
    data class ModeChange(val mode: Mode) : SessionControlChange()
}
