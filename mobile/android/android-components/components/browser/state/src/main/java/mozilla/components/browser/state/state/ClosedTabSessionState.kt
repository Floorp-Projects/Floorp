/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.EngineSessionState

/**
 * Value type that represents the state of a closed tab.
 */
data class ClosedTabSessionState(
    var id: String,
    var title: String,
    var url: String,
    var createdAt: Long,
    val engineSessionState: EngineSessionState? = null
)

typealias ClosedTab = ClosedTabSessionState
