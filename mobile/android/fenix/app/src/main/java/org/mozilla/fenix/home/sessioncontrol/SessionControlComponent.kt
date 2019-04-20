/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.graphics.Bitmap
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState

class SessionControlComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: SessionControlState = SessionControlState(emptyList(), Mode.Normal)
) :
    UIComponent<SessionControlState, SessionControlAction, SessionControlChange>(
        bus.getManagedEmitter(SessionControlAction::class.java),
        bus.getSafeManagedObservable(SessionControlChange::class.java)
    ) {

    override val reducer: (SessionControlState, SessionControlChange) -> SessionControlState = { state, change ->
        when (change) {
            is SessionControlChange.TabsChange -> state.copy(tabs = change.tabs)
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

data class Tab(
    val sessionId: String,
    val url: String,
    val hostname: String,
    val title: String,
    val selected: Boolean,
    val thumbnail: Bitmap? = null
)

sealed class Mode {
    object Normal : Mode()
    object Private : Mode()
}

data class SessionControlState(
    val tabs: List<Tab>,
    val mode: Mode
) : ViewState

sealed class TabAction : Action {
    object SaveTabGroup : TabAction()
    object MenuTapped : TabAction()
    object Add : TabAction()
    data class CloseAll(val private: Boolean) : TabAction()
    data class Select(val sessionId: String) : TabAction()
    data class Close(val sessionId: String) : TabAction()
    data class Share(val sessionId: String) : TabAction()
    object PrivateBrowsingLearnMore : TabAction()
}

sealed class SessionControlAction : Action {
    data class Tab(val action: TabAction) : SessionControlAction()
}

fun Observer<SessionControlAction>.onNext(tabAction: TabAction) {
    onNext(SessionControlAction.Tab(tabAction))
}

sealed class SessionControlChange : Change {
    data class TabsChange(val tabs: List<Tab>) : SessionControlChange()
    data class ModeChange(val mode: Mode) : SessionControlChange()
}
