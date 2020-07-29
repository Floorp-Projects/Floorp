/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * Interface for states that contain a [ContentState] and can be accessed via an [id].
 *
 * @property id the unique id of the session.
 * @property content the [ContentState] of this session.
 * @property trackingProtection the [TrackingProtectionState] of this session.
 * @property engineState the [EngineState] of this session.
 * @property extensionState a map of extension id and web extension states
 * specific to this [SessionState].
 * @property contextId the session context ID of the session. The session context ID specifies the
 * contextual identity to use for the session's cookie store.
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Work_with_contextual_identities
 * @property source the [Source] of this session to describe how and why it was created.
 * @property crashed Whether this session has crashed. In conjunction with a `concept-engine`
 * implementation that uses a multi-process architecture, single sessions can crash without crashing
 * the whole app. A crashed session may still be operational (since the underlying engine implementation
 * has recovered its content process), but further action may be needed to restore the last state
 * before the session has crashed (if desired).
 */
interface SessionState {
    val id: String
    val content: ContentState
    val trackingProtection: TrackingProtectionState
    val engineState: EngineState
    val extensionState: Map<String, WebExtensionState>
    val contextId: String?
    val source: Source
    val crashed: Boolean

    /**
     * Copy the class and override some parameters.
     */
    @Suppress("LongParameterList")
    fun createCopy(
        id: String = this.id,
        content: ContentState = this.content,
        trackingProtection: TrackingProtectionState = this.trackingProtection,
        engineState: EngineState = this.engineState,
        extensionState: Map<String, WebExtensionState> = this.extensionState,
        contextId: String? = this.contextId,
        crashed: Boolean = this.crashed
    ): SessionState

    /**
     * Represents the origin of a session to describe how and why it was created.
     */
    enum class Source {
        /**
         * Created to handle an ACTION_SEND (share) intent
         */
        ACTION_SEND,

        /**
         * Created to handle an ACTION_SEARCH and ACTION_WEB_SEARCH intent
         */
        ACTION_SEARCH,

        /**
         * Created to handle an ACTION_VIEW intent
         */
        ACTION_VIEW,

        /**
         * Created to handle a CustomTabs intent
         */
        CUSTOM_TAB,

        /**
         * User interacted with the home screen
         */
        HOME_SCREEN,

        /**
         * User interacted with a menu
         */
        MENU,

        /**
         * User opened a new tab
         */
        NEW_TAB,

        /**
         * Default value and for testing purposes
         */
        NONE,

        /**
         * Default value and for testing purposes
         */
        TEXT_SELECTION,

        /**
         * User entered a URL or search term
         */
        USER_ENTERED,

        /**
         * This session was restored
         */
        RESTORED
    }
}
