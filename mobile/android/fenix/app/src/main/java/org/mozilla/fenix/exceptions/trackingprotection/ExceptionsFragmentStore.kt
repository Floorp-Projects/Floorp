/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [ExceptionsFragmentState] and applying [ExceptionsFragmentAction]s.
 */
class ExceptionsFragmentStore(
    initialState: ExceptionsFragmentState = ExceptionsFragmentState(),
    middlewares: List<Middleware<ExceptionsFragmentState, ExceptionsFragmentAction>> = emptyList()
) : Store<ExceptionsFragmentState, ExceptionsFragmentAction>(
    initialState,
    ::exceptionsStateReducer,
    middlewares
)

/**
 * Actions to dispatch through the `ExceptionsStore` to modify `ExceptionsState` through the reducer.
 */
sealed class ExceptionsFragmentAction : Action {
    data class Change(val list: List<TrackingProtectionException>) : ExceptionsFragmentAction()
}

/**
 * The state for the Exceptions Screen
 * @property items List of exceptions to display
 */
data class ExceptionsFragmentState(val items: List<TrackingProtectionException> = emptyList()) : State

/**
 * The ExceptionsState Reducer.
 */
private fun exceptionsStateReducer(
    state: ExceptionsFragmentState,
    action: ExceptionsFragmentAction
): ExceptionsFragmentState {
    return when (action) {
        is ExceptionsFragmentAction.Change -> state.copy(items = action.list)
    }
}
