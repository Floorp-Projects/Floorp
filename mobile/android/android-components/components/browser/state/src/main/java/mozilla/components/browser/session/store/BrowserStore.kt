/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.store

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import mozilla.components.browser.session.action.Action
import mozilla.components.browser.session.reducer.BrowserReducers
import mozilla.components.browser.session.state.BrowserState
import java.util.concurrent.Executors

typealias Reducer = (BrowserState, Action) -> BrowserState
typealias Observer = (BrowserState) -> Unit

/**
 * The [BrowserStore] holds the [BrowserState] (state tree).
 *
 * The only way to change the [BrowserState] inside [BrowserStore] is to dispatch an [Action] on it.
 */
class BrowserStore(
    initialState: BrowserState
) {
    private val reducers: List<Reducer> = BrowserReducers.get()
    private val storeContext = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
    private val storeScope = CoroutineScope(storeContext)
    private val subscriptions: MutableList<Subscription> = mutableListOf()
    private var currentState = initialState

    /**
     * Dispatch an [Action] to the store in order to trigger a [BrowserState] change.
     *
     *
     */
    fun dispatch(action: Action) = storeScope.launch(storeContext) {
        dispatchInternal(action)
    }

    @Synchronized
    private fun dispatchInternal(action: Action) {
        val newState = reduceState(currentState, action, reducers)

        if (newState == currentState) {
            // Nothing has changed.
            return
        }

        currentState = newState

        synchronized(subscriptions) {
            subscriptions.forEach { it.observer.invoke(currentState) }
        }
    }

    /**
     * Returns the current state.
     */
    val state: BrowserState
        get() = currentState

    /**
     * Registers an observer function that will be invoked whenever the state changes.
     *
     * @param receiveInitialState If true the observer function will be invoked immediately with the current state.
     * @return A subscription object that can be used to unsubscribe from further state changes.
     */
    fun observe(receiveInitialState: Boolean = true, observer: Observer): Subscription {
        val subscription = Subscription(observer, store = this)

        synchronized(subscriptions) {
            subscriptions.add(subscription)
        }

        if (receiveInitialState) {
            observer.invoke(currentState)
        }

        return subscription
    }

    private fun removeSubscription(subscription: Subscription) {
        synchronized(subscriptions) {
            subscriptions.remove(subscription)
        }
    }

    /**
     * A [Subscription] is returned whenever an observer is registered via the [observe] method. Calling [unsubscribe]
     * on the [Subscription] will unregister the observer.
     */
    class Subscription internal constructor(
        internal val observer: Observer,
        private val store: BrowserStore
    ) {
        var binding: Binding? = null

        fun unsubscribe() {
            store.removeSubscription(this)

            binding?.unbind()
        }

        interface Binding {
            fun unbind()
        }
    }
}

private fun reduceState(state: BrowserState, action: Action, reducers: List<Reducer>): BrowserState {
    var current = state

    reducers.forEach { reducer ->
        current = reducer.invoke(current, action)
    }

    return current
}
