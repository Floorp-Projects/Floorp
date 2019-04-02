/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.ViewGroup
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.mvi.ViewState
import org.mozilla.fenix.settings.PhoneFeature

class QuickSettingsComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: QuickSettingsState
) : UIComponent<QuickSettingsState, QuickSettingsAction, QuickSettingsChange>(
    bus.getManagedEmitter(QuickSettingsAction::class.java),
    bus.getSafeManagedObservable(QuickSettingsChange::class.java)
) {
    override val reducer: (QuickSettingsState, QuickSettingsChange) -> QuickSettingsState = { state, change ->
        when (change) {
            is QuickSettingsChange.Change -> {
                state.copy(
                    mode = QuickSettingsState.Mode.Normal(
                        change.url,
                        change.isSecured,
                        change.isSiteInExceptionList
                    )
                )
            }
            is QuickSettingsChange.PermissionGranted -> {
                state.copy(
                    mode = QuickSettingsState.Mode.ActionLabelUpdated(change.phoneFeature)
                )
            }
            QuickSettingsChange.PromptRestarted -> {
                state.copy(
                    mode = QuickSettingsState.Mode.CheckPendingFeatureBlockedByAndroid
                )
            }
        }
    }

    override fun initView(): UIView<QuickSettingsState, QuickSettingsAction, QuickSettingsChange> {
        return QuickSettingsUIView(container, actionEmitter, changesObservable, container)
    }

    init {
        render(reducer)
    }
}

data class QuickSettingsState(val mode: Mode) : ViewState {
    sealed class Mode {
        data class Normal(val url: String, val isSecured: Boolean, val isSiteInExceptionList: Boolean) : Mode()
        data class ActionLabelUpdated(val phoneFeature: PhoneFeature) : Mode()
        object CheckPendingFeatureBlockedByAndroid : Mode()
    }
}

sealed class QuickSettingsAction : Action {
    data class SelectBlockedByAndroid(val permissions: Array<String>) : QuickSettingsAction()
    object DismissDialog : QuickSettingsAction()
}

sealed class QuickSettingsChange : Change {
    data class Change(
        val url: String,
        val isSecured: Boolean,
        val isSiteInExceptionList: Boolean
    ) : QuickSettingsChange()

    data class PermissionGranted(val phoneFeature: PhoneFeature) : QuickSettingsChange()
    object PromptRestarted : QuickSettingsChange()
}
