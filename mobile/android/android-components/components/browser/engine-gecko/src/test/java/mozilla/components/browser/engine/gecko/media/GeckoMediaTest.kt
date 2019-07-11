/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.media

import mozilla.components.concept.engine.media.Media
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.verify
import org.mozilla.geckoview.MediaElement

class GeckoMediaTest {
    @Test
    fun `Playback state is updated from MediaDelegate`() {
        val mediaElement: MediaElement = mock()

        val media = GeckoMedia(mediaElement)

        val captor = argumentCaptor<MediaElement.Delegate>()
        verify(mediaElement).delegate = captor.capture()

        val delegate = captor.value

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_PLAYING)
        assertEquals(Media.PlaybackState.PLAYING, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_SEEKING)
        assertEquals(Media.PlaybackState.SEEKING, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_WAITING)
        assertEquals(Media.PlaybackState.WAITING, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_PAUSE)
        assertEquals(Media.PlaybackState.PAUSE, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_PLAY)
        assertEquals(Media.PlaybackState.PLAY, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_SEEKED)
        assertEquals(Media.PlaybackState.SEEKED, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_STALLED)
        assertEquals(Media.PlaybackState.STALLED, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_SUSPEND)
        assertEquals(Media.PlaybackState.SUSPENDED, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_ABORT)
        assertEquals(Media.PlaybackState.ABORT, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_EMPTIED)
        assertEquals(Media.PlaybackState.EMPTIED, media.playbackState)

        delegate.onPlaybackStateChange(mediaElement, MediaElement.MEDIA_STATE_ENDED)
        assertEquals(Media.PlaybackState.ENDED, media.playbackState)
    }

    @Test
    fun `GeckoMedia exposes GeckoMediaController`() {
        val mediaElement: MediaElement = mock()

        val media = GeckoMedia(mediaElement)

        assertTrue(media.controller is GeckoMediaController)
    }
}
