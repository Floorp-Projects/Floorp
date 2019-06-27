package mozilla.components.browser.state.state

/**
 * Interface for states that contain a [ContentState] and can be accessed via an [id].
 *
 * @property id the unique id of the session.
 * @property content the [ContentState] of this session.
 */
interface SessionState {
    val id: String
    val content: ContentState
}
