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
     *
     * @property session The [Session] with currently playing media.
     * @property media The playing [Media] of the [Session].
     */
    data class Playing(
        val session: Session,
        val media: List<Media>
    ) : MediaState()

    /**
     * Paused: [media] of [session] is currently paused.
     *
     * @property session The [Session] with currently paused media.
     * @property media The paused [Media] of the [Session].
     */
    data class Paused(
        val session: Session,
        val media: List<Media>
    ) : MediaState()
}
