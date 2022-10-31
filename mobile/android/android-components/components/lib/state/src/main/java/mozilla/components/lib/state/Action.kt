/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

/**
 * Generic interface for actions to be dispatched on a [Store].
 *
 * Actions are used to send data from the application to a [Store]. The [Store] will use the [Action] to
 * derive a new [State]. Actions should describe what happened, while [Reducer]s will describe how the
 * state changes.
 */
interface Action
