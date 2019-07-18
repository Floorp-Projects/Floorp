/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Class representing an exception item
 * @property url Host of the exception
 */
data class ExceptionsItem(val url: String)

/**
 * The [Store] for holding the [ExceptionsState] and applying [ExceptionsAction]s.
 */
class ExceptionsStore(initialState: ExceptionsState) :
    Store<ExceptionsState, ExceptionsAction>(initialState, ::exceptionsStateReducer)

/**
 * Actions to dispatch through the `ExceptionsStore` to modify `ExceptionsState` through the reducer.
 */
sealed class ExceptionsAction : Action {
    data class Change(val list: List<ExceptionsItem>) : ExceptionsAction()
}

/**
 * The state for the Exceptions Screen
 * @property items List of exceptions to display
 */
data class ExceptionsState(val items: List<ExceptionsItem>) : State

/**
 * The ExceptionsState Reducer.
 */
fun exceptionsStateReducer(state: ExceptionsState, action: ExceptionsAction): ExceptionsState {
    return when (action) {
        is ExceptionsAction.Change -> state.copy(items = action.list)
    }
}
