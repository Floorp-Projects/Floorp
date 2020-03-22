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
 */
interface SessionState {
    val id: String
    val content: ContentState
    val trackingProtection: TrackingProtectionState
    val engineState: EngineState
    val extensionState: Map<String, WebExtensionState>
    val contextId: String?
}
