/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.mvi

import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.Disposable
import io.reactivex.schedulers.Schedulers

abstract class UIComponent<S : ViewState, A : Action, C : Change>(
    protected val actionEmitter: Observer<A>,
    protected val changesObservable: Observable<C>
) {

    abstract var initialState: S
    abstract val reducer: Reducer<S, C>

    open val uiView: UIView<S, A, C> by lazy { initView() }

    abstract fun initView(): UIView<S, A, C>
    open fun getContainerId() = uiView.containerId
    /**
     * Render the ViewState to the View through the Reducer
     */
    fun render(reducer: Reducer<S, C>): Disposable =
            internalRender(reducer)
            .subscribe(uiView.updateView())

    fun internalRender(reducer: Reducer<S, C>): Observable<S> =
        changesObservable
            .scan(initialState, reducer)
            .distinctUntilChanged()
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
}
