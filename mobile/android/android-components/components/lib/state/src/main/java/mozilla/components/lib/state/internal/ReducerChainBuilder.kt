/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.internal

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Reducer
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Builder to lazily create a function that will invoke the chain of [middleware] and finally the
 * [reducer].
 */
internal class ReducerChainBuilder<S : State, A : Action>(
    private val storeThreadFactory: StoreThreadFactory,
    private val reducer: Reducer<S, A>,
    private val middleware: List<Middleware<S, A>>,
) {
    private var chain: ((A) -> Unit)? = null

    /**
     * Returns a function that will invoke the chain of [middleware] and the [reducer] for the given
     * [Store].
     */
    fun get(store: Store<S, A>): (A) -> Unit {
        chain?.let { return it }

        return build(store).also {
            chain = it
        }
    }

    private fun build(store: Store<S, A>): (A) -> Unit {
        val context = object : MiddlewareContext<S, A> {
            override val state: S
                get() = store.state

            override fun dispatch(action: A) {
                get(store).invoke(action)
            }

            override val store: Store<S, A>
                get() = store
        }

        var chain: (A) -> Unit = { action ->
            val state = reducer(store.state, action)
            store.transitionTo(state)
        }

        val threadCheck: Middleware<S, A> = { _, next, action ->
            storeThreadFactory.assertOnThread()
            next(action)
        }

        (middleware.reversed() + threadCheck).forEach { middleware ->
            val next = chain
            chain = { action -> middleware(context, next, action) }
        }

        return chain
    }
}
