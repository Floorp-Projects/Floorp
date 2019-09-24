/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.ext

import mozilla.components.concept.engine.media.Media

/**
 * The minimum duration (in seconds) for media so that we bother with showing a media notification.
 */
private const val MINIMUM_DURATION_SECONDS = 5

/**
 * Pauses all [Media] in this list.
 */
internal fun List<Media>.pause() {
    forEach { it.controller.pause() }
}

/**
 * Plays all [Media] in this list.
 */
internal fun List<Media>.play() {
    forEach { it.controller.play() }
}

/**
 * Does this list contain [Media] that is sufficient long to justify showing media controls for it?
 */
internal fun List<Media>.hasMediaWithSufficientLongDuration(): Boolean {
    forEach { media ->
        if (media.metadata.duration < 0) {
            return true
        }

        if (media.metadata.duration > MINIMUM_DURATION_SECONDS) {
            return true
        }
    }

    return false
}
