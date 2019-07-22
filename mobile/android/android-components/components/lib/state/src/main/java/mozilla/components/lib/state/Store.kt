/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

import android.os.Handler
import android.os.Looper
import androidx.annotation.CheckResult
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.lang.ref.WeakReference
import java.util.concurrent.Executors

/**
 * Reducers specify how the application's [State] changes in response to [Action]s sent to the [Store].
 *
 * Remember that actions only describe what happened, but don't describe how the application's state changes.
 * Reducers will commonly consist of a `when` statement returning different copies of the [State].
 */
typealias Reducer<S, A> = (S, A) -> S

/**
 * Listener called when the state changes in the [Store].
 */
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
    private val subscriptions = mutableSetOf<Subscription<S, A>>()
    private val exceptionHandler = CoroutineExceptionHandler { _, throwable ->
        // We want exceptions in the reducer to crash the app and not get silently ignored. Therefore we rethrow the
        // exception on the main thread.
        Handler(Looper.getMainLooper()).postAtFrontOfQueue {
            throw StoreException("Exception while reducing state", throwable)
        }

        // Once an exception happened we do not want to accept any further actions. So let's cancel the scope which
        // will cancel all jobs and not accept any new ones.
        scope.cancel()
    }
    private val dispatcherWithExceptionHandler = dispatcher + exceptionHandler
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
     * It's the responsibility of the caller to keep track of the returned [Subscription] and call
     * [Subscription.unsubscribe] to stop observing and avoid potentially leaking memory by keeping an unused [Observer]
     * registered. It's is recommend to use one of the `observe` extension methods that unsubscribe automatically.
     *
     * The created [Subscription] is in paused state until explicitly resumed by calling [Subscription.resume].
     * While paused the [Subscription] will not receive any state updates. Once resumed the [observer]
     * will get invoked immediately with the latest state.
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

        return subscription
    }

    /**
     * Dispatch an [Action] to the store in order to trigger a [State] change.
     */
    fun dispatch(action: A) = scope.launch(dispatcherWithExceptionHandler) {
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
            subscriptions.forEach { subscription -> subscription.dispatch(newState) }
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
        private var active = false

        /**
         * Resumes the [Subscription]. The [Observer] will get notified for every state change.
         * Additionally it will get invoked immediately with the latest state.
         */
        @Synchronized
        fun resume() {
            active = true

            storeReference.get()?.state?.let(observer)
        }

        /**
         * Pauses the [Subscription]. The [Observer] will not get notified when the state changes
         * until [resume] is called.
         */
        @Synchronized
        fun pause() {
            active = false
        }

        @Synchronized
        internal fun dispatch(state: S) {
            if (active) {
                observer.invoke(state)
            }
        }

        /**
         * Unsubscribe from the [Store].
         *
         * Calling this method will clear all references and the subscription will not longer be
         * active.
         */
        @Synchronized
        fun unsubscribe() {
            active = false

            storeReference.get()?.removeSubscription(this)
            storeReference.clear()

            binding?.unbind()
        }

        interface Binding {
            fun unbind()
        }
    }
}

/**
 * Exception for otherwise unhandled errors caught while reducing state or
 * while managing/notifying observers.
 */
class StoreException(val msg: String, val e: Throwable? = null) : Exception(msg, e)
