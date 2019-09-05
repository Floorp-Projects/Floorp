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
