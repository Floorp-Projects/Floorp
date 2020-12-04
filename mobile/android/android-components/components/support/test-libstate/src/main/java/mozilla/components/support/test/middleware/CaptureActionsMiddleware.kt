/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.middleware

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import kotlin.reflect.KClass

/**
 * A [Middleware] implementation for unit tests that want to inspect actions dispatched on a `Store`
 */
class CaptureActionsMiddleware<S : State, A : Action> : Middleware<S, A> {
    private val capturedActions = mutableListOf<A>()

    @Synchronized
    override fun invoke(context: MiddlewareContext<S, A>, next: (A) -> Unit, action: A) {
        capturedActions.add(action)
        next(action)
    }

    /**
     * Returns the first action of type [clazz] that was dispatched on the store. Throws
     * [AssertionError] if no such action was dispatched.
     */
    @Synchronized
    @Suppress("UNCHECKED_CAST")
    fun <X : A> findAction(clazz: KClass<X>): X {
        return capturedActions.firstOrNull { it.javaClass == clazz.java } as? X
            ?: throw AssertionError("No action of type $clazz found")
    }

    /**
     * Executes the given [block] with the first action of type [clazz] that was dispatched on the
     * store. Throws [AssertionError] if no such action was dispatched.
     */
    fun <X : A> assertFirstAction(clazz: KClass<X>, block: (X) -> Unit) {
        val action = findAction(clazz)
        block(action)
    }

    /**
     * Asserts that no action of type [clazz] was dispatched. Throws [AssertionError] if a matching
     * action was found.
     */
    fun <X : A> assertNotDispatched(clazz: KClass<X>) {
        if (!capturedActions.none { it.javaClass == clazz.java }) {
            throw AssertionError("Action of type $clazz was dispatched: ${findAction(clazz)}")
        }
    }

    /**
     * Resets the remembered list of actions.
     *
     * Usually this is called between test runs to avoid verifying actions of a previous test methods.
     */
    @Synchronized
    fun reset() {
        capturedActions.clear()
    }
}
