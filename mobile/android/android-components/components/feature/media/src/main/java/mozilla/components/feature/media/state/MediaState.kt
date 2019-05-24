/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.state

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.media.Media

/**
 * Accumulated state of all [Media] of all [Session]s.
 */
sealed class MediaState {
    /**
     * None: No media state.
     */
    object None : MediaState() {
        override fun toString(): String = "MediaState.None"
    }

    /**
     * Playing: [media] of [session] is currently playing.
     */
    data class Playing(
        val session: Session,
        val media: List<Media>
    ) : MediaState()

    /**
     * Paused: [media] of [session] is currently paused.
     */
    data class Paused(
        val session: Session,
        val media: List<Media>
    ) : MediaState()
}
