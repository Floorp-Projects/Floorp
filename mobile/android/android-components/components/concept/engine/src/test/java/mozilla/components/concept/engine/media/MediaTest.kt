/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.media

import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class MediaTest {
    @Test
    fun `Observer is notified when playback state is changed`() {
        val media = FakeMedia()

        var observedMedia: Media? = null
        var observedState: Media.PlaybackState? = null

        val observer = object : Media.Observer {
            override fun onPlaybackStateChanged(media: Media, playbackState: Media.PlaybackState) {
                observedMedia = media
                observedState = playbackState
            }
        }

        media.register(observer)

        media.playbackState = Media.PlaybackState.PLAYING

        assertEquals(Media.PlaybackState.PLAYING, observedState)
        assertEquals(media, observedMedia)

        media.playbackState = Media.PlaybackState.SEEKING

        assertEquals(Media.PlaybackState.SEEKING, observedState)
        assertEquals(media, observedMedia)

        media.playbackState = Media.PlaybackState.PAUSE

        assertEquals(Media.PlaybackState.PAUSE, observedState)
        assertEquals(media, observedMedia)
    }

    @Test
    fun `Default playback state is unknown`() {
        val media = FakeMedia()
        assertEquals(Media.PlaybackState.UNKNOWN, media.playbackState)
    }

    @Test
    fun `Media-Observer has default implementation`() {
        val defaultObserver = object : Media.Observer {}

        val media: Media = mock()

        defaultObserver.onPlaybackStateChanged(media, Media.PlaybackState.SEEKED)
    }

    @Test
    fun `Observer is not notified about same playback state twice in a row`() {
        val observer: Media.Observer = mock()

        val media = FakeMedia()
        media.register(observer)

        media.playbackState = Media.PlaybackState.SEEKING
        media.playbackState = Media.PlaybackState.SEEKING

        verify(observer, times(1)).onPlaybackStateChanged(any(), any())
    }
}

private class FakeMedia : Media() {
    override val controller: Controller = mock()
    override val metadata: Metadata = mock()
}
