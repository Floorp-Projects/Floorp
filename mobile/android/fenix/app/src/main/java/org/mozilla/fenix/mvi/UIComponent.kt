/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.mvi

import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.Disposable
import io.reactivex.schedulers.Schedulers

abstract class UIComponent<S: ViewState, A: Action, C: Change>(open val bus: ActionBusFactory) {
    abstract var initialState: S
    abstract val reducer: Reducer<S, C>
    val uiView: UIView<S> by lazy { initView() }

    abstract fun initView(): UIView<S>
    open fun getContainerId() = uiView.containerId
    inline fun <reified A: Action> getUserInteractionEvents(): Observable<A> = bus.getSafeManagedObservable(A::class.java)
    inline fun <reified C: Change> getModelChangeEvents(): Observable<C> = bus.getSafeManagedObservable(C::class.java)

    /**
     * Render the ViewState to the View through the Reducer
     */
    inline fun <reified C : Change> render(noinline reducer: Reducer<S, C>): Disposable =
        bus.getSafeManagedObservable(C::class.java)
            .scan(initialState, reducer)
            .distinctUntilChanged()
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .subscribe(uiView.updateView())
}
