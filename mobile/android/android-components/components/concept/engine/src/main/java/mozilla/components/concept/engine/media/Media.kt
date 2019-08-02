/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.media

import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import kotlin.properties.Delegates

/**
 * Value type that represents a media element that is present on the currently displayed page in a session.
 */
abstract class Media(
    delegate: Observable<Media.Observer> = ObserverRegistry()
) : Observable<Media.Observer> by delegate {
    /**
     * The current [PlaybackState] of this media element.
     */
    var playbackState: PlaybackState by Delegates.observable(PlaybackState.UNKNOWN) {
        _, old, new -> notifyObservers(old, new) { onPlaybackStateChanged(this@Media, new) }
    }

    /**
     * The [Controller] for controlling playback of this media element.
     */
    abstract val controller: Controller

    /**
     * The [Metadata]
     */
    abstract val metadata: Metadata

    /**
     * Interface to be implemented by classes that want to observe a media element.
     */
    interface Observer {
        fun onPlaybackStateChanged(media: Media, playbackState: PlaybackState) = Unit
        fun onMetadataChanged(media: Media, metadata: Metadata) = Unit
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
         * Plays the media.
         */
        fun play()

        /**
         * Seek the media to a given time.
         */
        fun seek(time: Double)

        /**
         * Mutes/Unmutes the media.
         */
        fun setMuted(muted: Boolean)
    }

    // Implementation note: This is a 1:1 mapping of the playback states that GeckoView notifies us about. Maybe we
    // want to map this to a simpler model that doesn't contain all the in-between states?
    // For example we could try to map this to a PlaybackState of Android's media session right here.
    // https://developer.android.com/reference/android/media/session/PlaybackState
    enum class PlaybackState {
        /**
         * Unknown. No state has been received from the engine yet.
         */
        UNKNOWN,

        /**
         * The media is no longer paused, as a result of the play method, or the autoplay attribute.
         */
        PLAY,

        /**
         * Sent when the media has enough data to start playing, after the play event, but also when recovering from
         * being stalled, when looping media restarts, and after seeked, if it was playing before seeking.
         */
        PLAYING,

        /**
         * Sent when the playback state is changed to paused.
         */
        PAUSE,

        /**
         * Sent when playback completes.
         */
        ENDED,

        /**
         * Sent when a seek operation begins.
         */
        SEEKING,

        /**
         * Sent when a seek operation completes.
         */
        SEEKED,

        /**
         * Sent when the user agent is trying to fetch media data, but data is unexpectedly not forthcoming.
         */
        STALLED,

        /**
         * Sent when loading of the media is suspended. This may happen either because the download has completed or
         * because it has been paused for any other reason.
         */
        SUSPENDED,

        /**
         * Sent when the requested operation (such as playback) is delayed pending the completion of another operation
         * (such as a seek).
         */
        WAITING,

        /**
         * Sent when playback is aborted; for example, if the media is playing and is restarted from the beginning,
         * this event is sent.
         */
        ABORT,

        /**
         * The media has become empty. For example, this event is sent if the media has already been loaded, and the
         * load() method is called to reload it.
         */
        EMPTIED,
    }

    /**
     * Metadata associated with [Media].
     *
     * @property duration Indicates the duration of the media in seconds.
     */
    data class Metadata(
        val duration: Double = -1.0
    )

    /**
     * Helper method to notify observers.
     */
    private fun notifyObservers(old: Any, new: Any, block: Observer.() -> Unit) {
        if (old != new) {
            notifyObservers(block)
        }
    }

    override fun toString(): String = "Media($playbackState)"
}
