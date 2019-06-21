/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Observer
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Registers an [Observer] function that will be invoked whenever the state changes. The [Store.Subscription]
 * will be bound to the passed in [LifecycleOwner]. Once the [Lifecycle] state changes to DESTROYED the [Observer] will
 * be unregistered automatically.
 *
 * Right after registering the [Observer] will be invoked with the current [State].
 */
fun <S : State, A : Action> Store<S, A>.observe(
    owner: LifecycleOwner,
    observer: Observer<S>
) {
    if (owner.lifecycle.currentState == Lifecycle.State.DESTROYED) {
        // This owner is already destroyed. No need to register.
        return
    }

    val subscription = observeManually(observer)

    subscription.binding = SubscriptionLifecycleBinding(owner, subscription).apply {
        owner.lifecycle.addObserver(this)
    }
}

/**
 * Registers an [Observer] function that will be invoked whenever the state changes. The [Store.Subscription]
 * will be bound to the passed in [View]. Once the [View] gets detached the [Observer] will be unregistered
 * automatically.
 *
 * Right after registering the [Observer] will be invoked with the current [State].
 */
fun <S : State, A : Action> Store<S, A>.observe(
    view: View,
    observer: Observer<S>
) {
    val subscription = observeManually(observer)

    subscription.binding = SubscriptionViewBinding(view, subscription).apply {
        view.addOnAttachStateChangeListener(this)
    }
}

/**
 * Registers an [Observer] function that will observe the store indefinitely.
 *
 * Right after registering the [Observer] will be invoked with the current [State].
 */
fun <S : State, A : Action> Store<S, A>.observeForever(
    observer: Observer<S>
) {
    observe(ProcessLifecycleOwner.get(), observer)
}

/**
 * GenericLifecycleObserver implementation to bind an observer to a Lifecycle.
 */
private class SubscriptionLifecycleBinding<S : State, A : Action>(
    private val owner: LifecycleOwner,
    private val subscription: Store.Subscription<S, A>
) : LifecycleObserver, Store.Subscription.Binding {
    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun onDestroy() {
        subscription.unsubscribe()
    }

    override fun unbind() {
        owner.lifecycle.removeObserver(this)
    }
}

/**
 * View.OnAttachStateChangeListener implementation to bind an observer to a View.
 */
private class SubscriptionViewBinding<S : State, A : Action>(
    private val view: View,
    private val subscription: Store.Subscription<S, A>
) : View.OnAttachStateChangeListener, Store.Subscription.Binding {
    override fun onViewAttachedToWindow(v: View?) = Unit

    override fun onViewDetachedFromWindow(view: View) {
        subscription.unsubscribe()
    }

    override fun unbind() {
        view.removeOnAttachStateChangeListener(this)
    }
}
