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
 * @property engineObserver the [EngineSession.Observer] linked to [engineSession]. It is
 * used to observe engine events and update the store. It should become obsolete, once the
 * migration to browser state is complete, as the engine will then have direct access to
 * the store.
 * @property crashed Whether this session has crashed. In conjunction with a `concept-engine`
 * implementation that uses a multi-process architecture, single sessions can crash without crashing
 * the whole app. A crashed session may still be operational (since the underlying engine implementation
 * has recovered its content process), but further action may be needed to restore the last state
 * before the session has crashed (if desired).
 * @property timestamp Timestamp of when the [EngineSession] was linked.
 * @property initialLoadFlags [EngineSession.LoadUrlFlags] to use for the first load of this session.
 * @property initializing whether or not the [EngineSession] is currently being initialized.
 */
data class EngineState(
    val engineSession: EngineSession? = null,
    val engineSessionState: EngineSessionState? = null,
    val initializing: Boolean = false,
    val engineObserver: EngineSession.Observer? = null,
    val crashed: Boolean = false,
    val timestamp: Long? = null,
    val initialLoadFlags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
)
