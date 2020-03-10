/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

/**
 * A [Middleware] sits between the store and the reducer. It provides an extension point between
 * dispatching an action, and the moment it reaches the reducer.
 *
 * A [Middleware] can rewrite an [Action], it can intercept an [Action], dispatch additional
 * [Action]s or perform side-effects when an [Action] gets dispatched.
 *
 * The [Store] will create a chain of [Middleware] instances and invoke them in order. Every
 * [Middleware] can decide to continue the chain (by calling `next`), intercept the chain (by not
 * invoking `next`). A [Middleware] has no knowledge of what comes before or after it in the chain.
 */
typealias Middleware<S, A> = (store: MiddlewareStore<S, A>, next: (A) -> Unit, action: A) -> Unit

/**
 * A simplified [Store] interface for the purpose of passing it to a [Middleware].
 */
interface MiddlewareStore<S, A> {
    /**
     * Returns the current state of the [Store].
     */
    val state: S

    /**
     * Dispatches an [Action] synchronously on the [Store].
     */
    fun dispatch(action: A)
}
