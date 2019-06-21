/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.state

import mozilla.components.lib.state.State

/**
 * Value type that represents the complete state of the browser/engine.
 *
 * @property sessions List of currently open sessions ("tabs").
 * @property selectedSessionId The ID of the currently selected session (present in the [sessions] list].
 */
data class BrowserState(
    val sessions: List<SessionState> = emptyList(),
    val selectedSessionId: String? = null
) : State
