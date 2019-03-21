/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.view.ViewGroup
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.mvi.ViewState

class QuickActionComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: QuickActionState = QuickActionState(false)
) : UIComponent<QuickActionState, QuickActionAction, QuickActionChange>(
    bus.getManagedEmitter(QuickActionAction::class.java),
    bus.getSafeManagedObservable(QuickActionChange::class.java)
) {

    override val reducer: Reducer<QuickActionState, QuickActionChange> = { state, change ->
        when (change) {
            is QuickActionChange.ReadableStateChange -> {
                state.copy(readable = change.readable)
            }
        }
    }

    override fun initView(): UIView<QuickActionState, QuickActionAction, QuickActionChange> =
        QuickActionUIView(container, actionEmitter, changesObservable)

    init {
        render(reducer)
    }
}

data class QuickActionState(val readable: Boolean) : ViewState

sealed class QuickActionAction : Action {
    object SharePressed : QuickActionAction()
    object DownloadsPressed : QuickActionAction()
    object BookmarkPressed : QuickActionAction()
    object ReadPressed : QuickActionAction()
}

sealed class QuickActionChange : Change {
    data class ReadableStateChange(val readable: Boolean) : QuickActionChange()
}
