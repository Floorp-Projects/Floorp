/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState

/**
 * Value type that holds the browser engine state of a session.
 *
 * @property engineSession the engine's representation of this session.
 * @property engineSessionState serializable and restorable state of an engine session, see
 * [EngineSession.saveState] and [EngineSession.restoreState].
 */
data class EngineState(
    val engineSession: EngineSession? = null,
    val engineSessionState: EngineSessionState? = null
)
