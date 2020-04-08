/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")
package mozilla.components.feature.media.ext

import android.support.v4.media.session.PlaybackStateCompat
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.media.Media

/**
 * The minimum duration (in seconds) for media so that we bother with showing a media notification.
 */
private const val MINIMUM_DURATION_SECONDS = 5

/**
 * Returns a list of [MediaState.Element.id] of playing media on the tab with the given [tabId].
 */
fun MediaState.getPlayingMediaIdsForTab(tabId: String?): List<String> {
    return this.elements[tabId]
        ?.filter { it.state == Media.State.PLAYING }
        ?.map { it.id }
        ?: emptyList()
}

/**
 * Find a [SessionState] with playing [Media] and return this [Pair] or `null` if no such
 * [SessionState] could be found.
 */
fun MediaState.findPlayingSession(): Pair<String, List<MediaState.Element>>? {
    elements.forEach { (session, media) ->
        val playingMedia = media.filter { it.state == Media.State.PLAYING }
        if (playingMedia.isNotEmpty()) {
            return Pair(session, playingMedia)
        }
    }
    return null
}

/**
 * Does this list contain [Media] that is sufficient long to justify showing media controls for it?
 */
internal fun List<MediaState.Element>.hasMediaWithSufficientLongDuration(): Boolean {
    forEach { element ->
        if (element.metadata.duration < 0) {
            return true
        }

        if (element.metadata.duration > MINIMUM_DURATION_SECONDS) {
            return true
        }
    }

    return false
}

/**
 * Does this list contain [Media] that has audible audio?
 */
internal fun List<MediaState.Element>.hasMediaWithAudibleAudio(): Boolean {
    return any { !it.volume.muted }
}

/**
 * Get the list of paused [Media] for the tab with the provided [tabId].
 */
fun MediaState.getPausedMedia(tabId: String?): List<MediaState.Element> {
    val media = elements[tabId] ?: emptyList()
    return media.filter { it.state == Media.State.PAUSED }
}

/**
 * Get the [SessionState] that caused this [MediaState] (if any).
 */
fun BrowserState.getActiveMediaTab(): SessionState? {
    return media.aggregate.activeTabId?.let { id -> findTabOrCustomTab(id) }
}

/**
 * Returns true if this [MediaState] is associated with a Custom Tab [SessionState].
 */
internal fun BrowserState.isMediaStateForCustomTab(): Boolean {
    return media.aggregate.activeTabId?.let { id ->
        findCustomTab(id) != null
    } ?: false
}

/**
 * Turns the [MediaState] into a [PlaybackStateCompat] to be used with a `MediaSession`.
 */
internal fun MediaState.toPlaybackState() =
    PlaybackStateCompat.Builder()
        .setActions(
            PlaybackStateCompat.ACTION_PLAY_PAUSE or
                PlaybackStateCompat.ACTION_PLAY or
                PlaybackStateCompat.ACTION_PAUSE)
        .setState(
            when (aggregate.state) {
                MediaState.State.PLAYING -> PlaybackStateCompat.STATE_PLAYING
                MediaState.State.PAUSED -> PlaybackStateCompat.STATE_PAUSED
                MediaState.State.NONE -> PlaybackStateCompat.STATE_NONE
            },
            // Time state not exposed yet:
            // https://github.com/mozilla-mobile/android-components/issues/2458
            PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN,
            when (aggregate.state) {
                // The actual playback speed is not exposed yet:
                // https://github.com/mozilla-mobile/android-components/issues/2459
                MediaState.State.PLAYING -> 1.0f
                else -> 0.0f
            })
        .build()

/**
 * Returns the list of active [MediaState.Element]s.
 */
fun MediaState.getActiveElements(): List<MediaState.Element> {
    val tabId = aggregate.activeTabId
    return if (tabId != null) {
        return elements[tabId]?.filter { it.id in aggregate.activeMedia } ?: emptyList()
    } else {
        emptyList()
    }
}

/**
 * If this state is [MediaState.State.PLAYING] then pause all playing [Media].
 */
fun MediaState.pauseIfPlaying() {
    if (aggregate.state == MediaState.State.PLAYING) {
        getActiveElements().pause()
    }
}

/**
 * If this state is [MediaState.State.PAUSED] then resume playing all paused [Media].
 */
fun MediaState.playIfPaused() {
    if (aggregate.state == MediaState.State.PAUSED) {
        getActiveElements().play()
    }
}
