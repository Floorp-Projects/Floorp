/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

/**
 * Reducers specify how the application's [State] changes in response to [Action]s sent to the [Store].
 *
 * Remember that actions only describe what happened, but don't describe how the application's state changes.
 * Reducers will commonly consist of a `when` statement returning different copies of the [State].
 */
typealias Reducer<S, A> = (S, A) -> S
