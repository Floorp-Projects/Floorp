/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.mvi

import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.Disposable
import io.reactivex.schedulers.Schedulers

abstract class UIComponent<S : ViewState, A : Action, C : Change>(
    protected val owner: Fragment,
    protected val actionEmitter: Observer<A>,
    protected val changesObservable: Observable<C>
) {

    abstract var initialState: S

    open val uiView: UIView<S, A, C> by lazy { initView() }

    abstract fun initView(): UIView<S, A, C>
    open fun getContainerId() = uiView.containerId
    abstract fun render(): Observable<S>
}

open class UIComponentViewModel<S : ViewState, A : Action, C : Change>(
    initialState: S,
    private val reducer: Reducer<S, C>
) : ViewModel() {

    private var currentState: S = initialState
    private var statesDisposable: Disposable? = null

    /**
     * Render the ViewState to the View through the Reducer
     */
    fun render(changesObservable: Observable<C>, uiView: UIView<S, A, C>): Observable<S> {
        val statesObservable = internalRender(changesObservable, reducer)
        statesDisposable = statesObservable
            .subscribe(uiView.updateView())
        return statesObservable
    }

    @Suppress("MemberVisibilityCanBePrivate")
    protected fun internalRender(changesObservable: Observable<C>, reducer: Reducer<S, C>): Observable<S> =
        changesObservable
            .scan(currentState, reducer)
            .distinctUntilChanged()
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .replay(1)
            .autoConnect(0)
            .doOnNext { currentState = it }

    override fun onCleared() {
        super.onCleared()
        statesDisposable?.dispose()
    }
}
