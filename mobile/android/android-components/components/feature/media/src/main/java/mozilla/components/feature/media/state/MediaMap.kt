/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.state

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.media.Media
import java.util.WeakHashMap

/**
 * Internal helper to keep a list from [Session] instances to its [Media] and make it searchable.
 */
internal class MediaMap {
    private val map = WeakHashMap<Session, List<Media>>()

    /**
     * Update the list of [Media] for the given [Session].
     */
    fun updateSessionMedia(session: Session, media: List<Media>) {
        if (media.isEmpty()) {
            map.remove(session)
        } else {
            map[session] = media
        }
    }

    /**
     * Find a [Session] with playing [Media] and return this [Pair] or `null` if no such [Session] could be found.
     */
    fun findPlayingSession(): Pair<Session, List<Media>>? {
        map.forEach { (session, media) ->
            val playingMedia = media.filter { it.state == Media.State.PLAYING }
            if (playingMedia.isNotEmpty()) {
                return Pair(session, playingMedia)
            }
        }
        return null
    }

    /**
     * Get the list of playing [Media] for the provided [Session].
     */
    fun getPlayingMedia(session: Session): List<Media> {
        val media = map[session] ?: emptyList()
        return media.filter { it.state == Media.State.PLAYING }
    }

    /**
     * Get the list of paused [Media] for the provided [Session].
     */
    fun getPausedMedia(session: Session): List<Media> {
        val media = map[session] ?: emptyList()
        return media.filter { it.state == Media.State.PAUSED }
    }

    /**
     * Get all media for the provided [Session].
     */
    fun getAllMedia(session: Session): List<Media> {
        return map[session] ?: emptyList()
    }
}
