/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import io.reactivex.Observable
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModel
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.mvi.ViewState

class SignInComponent(
    private val container: ViewGroup,
    owner: Fragment,
    bus: ActionBusFactory,
    override var initialState: SignInState =
        SignInState(false)
) : UIComponent<SignInState, SignInAction, SignInChange>(
    owner,
    bus.getManagedEmitter(SignInAction::class.java),
    bus.getSafeManagedObservable(SignInChange::class.java)
) {
    override fun initView(): UIView<SignInState, SignInAction, SignInChange> =
        SignInUIView(container, actionEmitter, changesObservable)

    override fun render(): Observable<SignInState> =
        ViewModelProvider(
            owner,
            SignInViewModel.Factory(initialState, changesObservable)
        ).get(SignInViewModel::class.java).render(uiView)

    init {
        render()
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

class SignInViewModel(initialState: SignInState, changesObservable: Observable<SignInChange>) :
    UIComponentViewModel<SignInState, SignInAction, SignInChange>(
        initialState, changesObservable, reducer
    ) {

    class Factory(
        private val initialState: SignInState,
        private val changesObservable: Observable<SignInChange>
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel?> create(modelClass: Class<T>): T =
            SignInViewModel(initialState, changesObservable) as T
    }

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
