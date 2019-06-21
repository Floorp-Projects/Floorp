/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

import androidx.annotation.CheckResult
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import java.lang.ref.WeakReference
import java.util.concurrent.Executors

typealias Reducer<S, A> = (S, A) -> S
typealias Observer<S> = (S) -> Unit

/**
 * A generic store holding an immutable [State].
 *
 * The [State] can only be modified by dispatching [Action]s which will create a new state and notify all registered
 * [Observer]s.
 *
 * @param initialState The initial state until a dispatched [Action] creates a new state.
 * @param reducer A function that gets the current [State] and [Action] passed in and will return a new [State].
 */
open class Store<S : State, A : Action>(
    initialState: S,
    private val reducer: Reducer<S, A>
) {
    private val dispatcher = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
    private val scope = CoroutineScope(dispatcher)
    private val subscriptions: MutableSet<Subscription<S, A>> = mutableSetOf()
    private var currentState = initialState

    /**
     * The current [State].
     */
    val state: S
        @Synchronized
        get() = currentState

    /**
     * Registers an [Observer] function that will be invoked whenever the [State] changes.
     *
     * Right after registering the [Observer] will be invoked with the current [State].
     *
     * It's the responsibility of the caller to keep track of the returned [Subscription] and call
     * [Subscription.unsubscribe] to stop observing and avoid potentially leaking memory by keeping an unused [Observer]
     * registered. It's is recommend to use one of the `observe` extension methods that unsubscribe automatically.
     *
     * @return A [Subscription] object that can be used to unsubscribe from further state changes.
     */
    @CheckResult(suggest = "observe")
    @Synchronized
    fun observeManually(observer: Observer<S>): Subscription<S, A> {
        val subscription = Subscription(observer, store = this)

        synchronized(subscriptions) {
            subscriptions.add(subscription)
        }

        observer.invoke(currentState)

        return subscription
    }

    /**
     * Dispatch an [Action] to the store in order to trigger a [State] change.
     */
    fun dispatch(action: A) = scope.launch(dispatcher) {
        dispatchInternal(action)
    }

    @Synchronized
    private fun dispatchInternal(action: A) {
        val newState = reducer(currentState, action)

        if (newState == currentState) {
            // Nothing has changed.
            return
        }

        currentState = newState

        synchronized(subscriptions) {
            subscriptions.forEach { it.observer.invoke(currentState) }
        }
    }

    private fun removeSubscription(subscription: Subscription<S, A>) {
        synchronized(subscriptions) {
            subscriptions.remove(subscription)
        }
    }

    /**
     * A [Subscription] is returned whenever an observer is registered via the [observeManually] method. Calling
     * [unsubscribe] on the [Subscription] will unregister the observer.
     */
    class Subscription<S : State, A : Action> internal constructor(
        internal val observer: Observer<S>,
        store: Store<S, A>
    ) {
        private val storeReference = WeakReference(store)
        internal var binding: Binding? = null

        fun unsubscribe() {
            storeReference.get()?.removeSubscription(this)

            binding?.unbind()
        }

        interface Binding {
            fun unbind()
        }
    }
}
