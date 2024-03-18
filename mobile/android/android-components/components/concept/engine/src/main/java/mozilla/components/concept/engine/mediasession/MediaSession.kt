/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.mediasession

import android.graphics.Bitmap

/**
 * Value type that represents a media session that is present on the currently displayed page in a session.
 */
class MediaSession {

    /**
     * The representation of a media element's metadata.
     *
     * @property source The media URI.
     * @property duration The media duration in seconds.
     * @property width The video width in device pixels.
     * @property height The video height in device pixels.
     * @property audioTrackCount The audio track count.
     * @property videoTrackCount The video track count.
     */
    data class ElementMetadata(
        val source: String? = null,
        val duration: Double = -1.0,
        val width: Long = 0L,
        val height: Long = 0L,
        val audioTrackCount: Int = 0,
        val videoTrackCount: Int = 0,
    ) {
        val portrait: Boolean
            get() = height > width
    }

    /**
     * The representation of a media session's metadata.
     *
     * @property title The media title string.
     * @property artist The media artist string.
     * @property album The media album string.
     * @property getArtwork Get the media artwork.
     */
    data class Metadata(
        val title: String? = null,
        val artist: String? = null,
        val album: String? = null,
        val getArtwork: (suspend () -> Bitmap?)?,
    )

    /**
     * Holds the details of the media session's playback state.
     *
     * @property duration The media duration in seconds.
     * @property position The current media playback position in seconds.
     * @property playbackRate The playback rate coefficient.
     */
    data class PositionState(
        val duration: Double = -1.0,
        val position: Double = 0.0,
        val playbackRate: Double = 0.0,
    )

    /**
     * Flags for supported media session features.
     *
     * Implementation note: This is a 1:1 mapping of the features that GeckoView notifies us about.
     * https://github.com/mozilla/gecko-dev/blob/master/mobile/android/geckoview/src/main/java/org/mozilla/geckoview/MediaSession.java
     */
    data class Feature(val flags: Long = 0) {
        companion object {
            const val NONE: Long = 0
            const val PLAY: Long = 1L shl 0
            const val PAUSE: Long = 1L shl 1
            const val STOP: Long = 1L shl 2
            const val SEEK_TO: Long = 1L shl 3
            const val SEEK_FORWARD: Long = 1L shl 4
            const val SEEK_BACKWARD: Long = 1L shl 5
            const val SKIP_AD: Long = 1L shl 6
            const val NEXT_TRACK: Long = 1L shl 7
            const val PREVIOUS_TRACK: Long = 1L shl 8
            const val FOCUS: Long = 1L shl 9
        }

        /**
         * Returns `true` if this [Feature] contains the [type].
         */
        fun contains(flag: Long): Boolean = (flags and flag) != 0L

        /**
         * Returns `true` if this is [Feature] equal to the [other] [Feature].
         */
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is Feature) return false
            if (flags != other.flags) return false
            return true
        }

        override fun hashCode() = flags.hashCode()
    }

    /**
     * A simplified media session playback state.
     */
    enum class PlaybackState {
        /**
         * Unknown. No state has been received from the engine yet.
         */
        UNKNOWN,

        /**
         * Playback of this [MediaSession] has stopped (either completed or aborted).
         */
        STOPPED,

        /**
         * This [MediaSession] is paused.
         */
        PAUSED,

        /**
         * This [MediaSession] is currently playing.
         */
        PLAYING,
    }

    /**
     * Controller for controlling playback of a media element.
     */
    interface Controller {
        /**
         * Pauses the media.
         */
        fun pause()

        /**
         * Stop playback for the media session.
         */
        fun stop()

        /**
         * Plays the media.
         */
        fun play()

        /**
         * Seek to a specific time.
         * Prefer using fast seeking when calling this in a sequence.
         * Don't use fast seeking for the last or only call in a sequence.
         *
         * @param time The time in seconds to move the playback time to.
         * @param fast Whether fast seeking should be used.
         */
        fun seekTo(time: Double, fast: Boolean)

        /**
         * Seek forward by a sensible number of seconds.
         */
        fun seekForward()

        /**
         * Seek backward by a sensible number of seconds.
         */
        fun seekBackward()

        /**
         * Select and play the next track.
         * Move playback to the next item in the playlist when supported.
         */
        fun nextTrack()

        /**
         * Select and play the previous track.
         * Move playback to the previous item in the playlist when supported.
         */
        fun previousTrack()

        /**
         * Skip the advertisement that is currently playing.
         */
        fun skipAd()

        /**
         * Set whether audio should be muted.
         * Muting audio is supported by default and does not require the media
         * session to be active.
         *
         * @param mute True if audio for this media session should be muted.
         */
        fun muteAudio(mute: Boolean)
    }
}
