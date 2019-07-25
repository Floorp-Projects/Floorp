/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.google.android.material.button.MaterialButton
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

class SignInUIView(
    container: ViewGroup,
    actionEmitter: Observer<SignInAction>,
    changesObservable: Observable<SignInChange>
) : UIView<SignInState, SignInAction, SignInChange>(container, actionEmitter, changesObservable) {

    override val view: MaterialButton = LayoutInflater.from(container.context)
        .inflate(R.layout.component_sign_in, container, true)
        .findViewById(R.id.bookmark_folders_sign_in)

    init {
        view.setOnClickListener {
            actionEmitter.onNext(SignInAction.ClickedSignIn)
        }
    }

    override fun updateView() = Consumer<SignInState> {
        view.visibility = if (it.signedIn) View.GONE else View.VISIBLE
    }
}
