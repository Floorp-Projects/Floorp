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
 */
interface SessionState {
    val id: String
    val content: ContentState
    val trackingProtection: TrackingProtectionState
    val engineState: EngineState
}
