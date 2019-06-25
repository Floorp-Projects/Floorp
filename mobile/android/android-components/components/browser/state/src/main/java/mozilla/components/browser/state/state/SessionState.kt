package mozilla.components.browser.state.state

/**
 * Interface for states that contain a [ContentState] and can be accessed via an [id].
 */
interface SessionState {
    val id: String
    val content: ContentState
}
