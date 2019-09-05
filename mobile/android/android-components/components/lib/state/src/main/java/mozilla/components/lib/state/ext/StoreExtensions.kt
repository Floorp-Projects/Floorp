/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.view.View
import androidx.annotation.MainThread
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.ReceiveChannel
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.buffer
import kotlinx.coroutines.flow.channelFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Observer
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Registers an [Observer] function that will be invoked whenever the state changes. The [Store.Subscription]
 * will be bound to the passed in [LifecycleOwner]. Once the [Lifecycle] state changes to DESTROYED the [Observer] will
 * be unregistered automatically.
 *
 * The [Observer] will get invoked with the current [State] as soon as the [Lifecycle] is in STARTED
 * state.
 */
@MainThread
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
 * Note that inside a `Fragment` using [observe] with a `viewLifecycleOwner` may be a better option.
 * Only use this implementation if you have only access to a [View] - especially if it can exist
 * outside of a `Fragment`.
 *
 * The [Observer] will get invoked with the current [State] as soon as [View] is attached.
 *
 * Once the [View] gets detached the [Observer] will get unregistered. It will NOT get automatically
 * registered again if the same [View] gets attached again.
 */
@MainThread
fun <S : State, A : Action> Store<S, A>.observe(
    view: View,
    observer: Observer<S>
) {
    val subscription = observeManually(observer)

    subscription.binding = SubscriptionViewBinding(view, subscription).apply {
        view.addOnAttachStateChangeListener(this)
    }

    if (view.isAttachedToWindow) {
        // This View is already attached. We can resume immediately and do not need to wait for
        // onViewAttachedToWindow() getting called.
        subscription.resume()
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
    observeManually(observer).resume()
}

/**
 * Creates a conflated [Channel] for observing [State] changes in the [Store].
 *
 * The advantage of a [Channel] is that [State] changes can be processed sequentially in order from
 * a single coroutine (e.g. on the main thread).
 *
 * @param owner A [LifecycleOwner] that will be used to determine when to pause and resume the store
 * subscription. When the [Lifecycle] is in STOPPED state then no [State] will be received. Once the
 * [Lifecycle] switches back to at least STARTED state then the latest [State] and further updates
 * will be received.
 */
@ExperimentalCoroutinesApi
@MainThread
fun <S : State, A : Action> Store<S, A>.channel(
    owner: LifecycleOwner = ProcessLifecycleOwner.get()
): ReceiveChannel<S> {
    if (owner.lifecycle.currentState == Lifecycle.State.DESTROYED) {
        // This owner is already destroyed. No need to register.
        throw IllegalArgumentException("Lifecycle is already DESTROYED")
    }

    val channel = Channel<S>(Channel.CONFLATED)

    val subscription = observeManually { state ->
        runBlocking { channel.send(state) }
    }

    subscription.binding = SubscriptionLifecycleBinding(owner, subscription).apply {
        owner.lifecycle.addObserver(this)
    }

    channel.invokeOnClose { subscription.unsubscribe() }

    return channel
}

/**
 * Creates a [Flow] for observing [State] changes in the [Store].
 *
 * @param owner An optional [LifecycleOwner] that will be used to determine when to pause and resume
 * the store subscription. When the [Lifecycle] is in STOPPED state then no [State] will be received.
 * Once the [Lifecycle] switches back to at least STARTED state then the latest [State] and further
 * updates will be emitted.
 */
@ExperimentalCoroutinesApi
@MainThread
fun <S : State, A : Action> Store<S, A>.flow(
    owner: LifecycleOwner? = null
): Flow<S> {
    return channelFlow {
        val subscription = observeManually { state ->
            runBlocking { send(state) }
        }

        if (owner == null) {
            subscription.resume()
        } else {
            subscription.binding = SubscriptionLifecycleBinding(owner, subscription).apply {
                owner.lifecycle.addObserver(this)
            }
        }

        awaitClose {
            subscription.unsubscribe()
        }
    }.buffer(Channel.CONFLATED)
}

/**
 * Launches a coroutine in a new [MainScope] and creates a [Flow] for observing [State] changes in
 * the [Store] in that scope. Invokes [block] inside that scope and passes the [Flow] to it.
 *
 * @param owner An optional [LifecycleOwner] that will be used to determine when to pause and resume
 * the store subscription. When the [Lifecycle] is in STOPPED state then no [State] will be received.
 * Once the [Lifecycle] switches back to at least STARTED state then the latest [State] and further
 * updates will be emitted.
 * @return The [CoroutineScope] [block] is getting executed in.
 */
@ExperimentalCoroutinesApi
@MainThread
fun <S : State, A : Action> Store<S, A>.flowScoped(
    owner: LifecycleOwner? = null,
    block: suspend (Flow<S>) -> Unit
): CoroutineScope {
    return MainScope().apply {
        launch {
            block(flow(owner))
        }
    }
}

/**
 * GenericLifecycleObserver implementation to bind an observer to a Lifecycle.
 */
private class SubscriptionLifecycleBinding<S : State, A : Action>(
    private val owner: LifecycleOwner,
    private val subscription: Store.Subscription<S, A>
) : LifecycleObserver, Store.Subscription.Binding {
    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onStart() {
        subscription.resume()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onStop() {
        subscription.pause()
    }

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
    override fun onViewAttachedToWindow(v: View?) {
        subscription.resume()
    }

    override fun onViewDetachedFromWindow(view: View) {
        subscription.unsubscribe()
    }

    override fun unbind() {
        view.removeOnAttachStateChangeListener(this)
    }
}
