/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.mvi

import android.view.View
import android.view.ViewGroup
import androidx.annotation.IdRes
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.extensions.LayoutContainer

abstract class UIView<S : ViewState, A : Action, C : Change>(
    private val container: ViewGroup,
    protected val actionEmitter: Observer<A>,
    protected val changesObservable: Observable<C>
) : LayoutContainer {

    abstract val view: View

    /**
     * Get the XML id for the UIView
     */
    @get:IdRes
    val containerId: Int
        get() = container.id

    /**
     * Provides container to empower Kotlin Android Extensions
     */
    override val containerView: View?
        get() = container

    /**
     * Show the UIView
     */
    open fun show() {
        view.visibility = View.VISIBLE
    }

    /**
     * Hide the UIView
     */
    open fun hide() {
        view.visibility = View.GONE
    }

    /**
     * Update the view from the ViewState
     */
    abstract fun updateView(): Consumer<S>
}
