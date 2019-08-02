/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import mozilla.components.concept.engine.media.Media
import org.mozilla.geckoview.MediaElement
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_ABORT
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_EMPTIED
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_ENDED
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_PAUSE
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_PLAY
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_PLAYING
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_SEEKED
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_SEEKING
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_STALLED
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_SUSPEND
import org.mozilla.geckoview.MediaElement.MEDIA_STATE_WAITING

/**
 * [Media] (`concept-engine`) implementation for GeckoView.
 */
internal class GeckoMedia(
    internal val mediaElement: MediaElement
) : Media() {
    override val controller: Controller = GeckoMediaController(mediaElement)

    override var metadata: Metadata = Metadata()
        internal set

    init {
        mediaElement.delegate = MediaDelegate(this)
    }

    override fun equals(other: Any?): Boolean {
        if (other == null || other !is GeckoMedia) {
            return false
        }

        return mediaElement == other.mediaElement
    }

    override fun hashCode(): Int {
        return mediaElement.hashCode()
    }
}

private class MediaDelegate(
    private val media: GeckoMedia
) : MediaElement.Delegate {
    @Suppress("ComplexMethod")
    override fun onPlaybackStateChange(mediaElement: MediaElement, mediaState: Int) {
        when (mediaState) {
            MEDIA_STATE_PLAY -> media.playbackState = Media.PlaybackState.PLAY
            MEDIA_STATE_PLAYING -> media.playbackState = Media.PlaybackState.PLAYING
            MEDIA_STATE_PAUSE -> media.playbackState = Media.PlaybackState.PAUSE
            MEDIA_STATE_ENDED -> media.playbackState = Media.PlaybackState.ENDED
            MEDIA_STATE_SEEKING -> media.playbackState = Media.PlaybackState.SEEKING
            MEDIA_STATE_SEEKED -> media.playbackState = Media.PlaybackState.SEEKED
            MEDIA_STATE_STALLED -> media.playbackState = Media.PlaybackState.STALLED
            MEDIA_STATE_SUSPEND -> media.playbackState = Media.PlaybackState.SUSPENDED
            MEDIA_STATE_WAITING -> media.playbackState = Media.PlaybackState.WAITING
            MEDIA_STATE_ABORT -> media.playbackState = Media.PlaybackState.ABORT
            MEDIA_STATE_EMPTIED -> media.playbackState = Media.PlaybackState.EMPTIED
        }
    }

    override fun onMetadataChange(mediaElement: MediaElement, metaData: MediaElement.Metadata) {
        media.metadata = Media.Metadata(metaData.duration)
    }

    override fun onReadyStateChange(mediaElement: MediaElement, readyState: Int) = Unit
    override fun onLoadProgress(mediaElement: MediaElement, progressInfo: MediaElement.LoadProgressInfo) = Unit
    override fun onVolumeChange(mediaElement: MediaElement, volume: Double, muted: Boolean) = Unit
    override fun onTimeChange(mediaElement: MediaElement, time: Double) = Unit
    override fun onPlaybackRateChange(mediaElement: MediaElement, rate: Double) = Unit
    override fun onFullscreenChange(mediaElement: MediaElement, fullscreen: Boolean) = Unit
    override fun onError(mediaElement: MediaElement, errorCode: Int) = Unit
}
