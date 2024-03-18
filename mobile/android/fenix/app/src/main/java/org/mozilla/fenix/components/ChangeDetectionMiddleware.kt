/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State

/**
 * A Middleware for detecting changes to a state property, and offering a callback that captures the action that changed
 * the property, the property before the change, and the property after the change.
 *
 * For example, this can be useful for debugging:
 * ```
 * val selectedTabChangedMiddleware: Middleware<BrowserState, BrowserAction> = ChangeDetectionMiddleware(
 *     val selector = { it.selectedTabId }
 *     val onChange = { actionThatCausedResult, preResult, postResult ->
 *         logger.debug("$actionThatCausedResult changed selectedTabId from $preResult to $postResult")
 *     }
 * ```
 *
 * @param selector A function to map from the State to the properties that are being inspected.
 * @param onChange A callback to react to changes to the properties defined by [selector].
 */
class ChangeDetectionMiddleware<S : State, A : Action, T>(
    private val selector: (S) -> T,
    private val onChange: (A, pre: T, post: T) -> Unit,
) : Middleware<S, A> {
    override fun invoke(context: MiddlewareContext<S, A>, next: (A) -> Unit, action: A) {
        val pre = selector(context.store.state)
        next(action)
        val post = selector(context.store.state)
        if (pre != post) {
            onChange(action, pre, post)
        }
    }
}
