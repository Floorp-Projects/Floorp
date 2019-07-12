/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.ViewGroup
import org.mozilla.fenix.mvi.ViewState
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.mvi.UIView

class SignInComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    viewModelProvider: UIComponentViewModelProvider<SignInState, SignInChange>
) : UIComponent<SignInState, SignInAction, SignInChange>(
    bus.getManagedEmitter(SignInAction::class.java),
    bus.getSafeManagedObservable(SignInChange::class.java),
    viewModelProvider
) {
    override fun initView(): UIView<SignInState, SignInAction, SignInChange> =
        SignInUIView(container, actionEmitter, changesObservable)

    init {
        bind()
    }
}

data class SignInState(val signedIn: Boolean) : ViewState

sealed class SignInAction : Action {
    object ClickedSignIn : SignInAction()
}

sealed class SignInChange : Change {
    object SignedIn : SignInChange()
    object SignedOut : SignInChange()
}

class SignInViewModel(
    initialState: SignInState
) : UIComponentViewModelBase<SignInState, SignInChange>(initialState, reducer) {
    companion object {
        val reducer = object : Reducer<SignInState, SignInChange> {
            override fun invoke(state: SignInState, change: SignInChange): SignInState {
                return when (change) {
                    SignInChange.SignedIn -> state.copy(signedIn = true)
                    SignInChange.SignedOut -> state.copy(signedIn = false)
                }
            }
        }
    }
}
