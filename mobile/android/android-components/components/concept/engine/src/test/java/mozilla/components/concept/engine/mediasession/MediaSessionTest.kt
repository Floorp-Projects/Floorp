/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.mediasession

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class MediaSessionTest {
    @Test
    fun `media session feature works correctly`() {
        var features = MediaSession.Feature()
        assertFalse(features.contains(MediaSession.Feature.PLAY))
        assertFalse(features.contains(MediaSession.Feature.PAUSE))
        assertFalse(features.contains(MediaSession.Feature.STOP))
        assertFalse(features.contains(MediaSession.Feature.SEEK_TO))
        assertFalse(features.contains(MediaSession.Feature.SEEK_FORWARD))
        assertFalse(features.contains(MediaSession.Feature.SEEK_BACKWARD))
        assertFalse(features.contains(MediaSession.Feature.SKIP_AD))
        assertFalse(features.contains(MediaSession.Feature.NEXT_TRACK))
        assertFalse(features.contains(MediaSession.Feature.PREVIOUS_TRACK))
        assertFalse(features.contains(MediaSession.Feature.FOCUS))

        features = MediaSession.Feature(MediaSession.Feature.PLAY + MediaSession.Feature.PAUSE)
        assert(features.contains(MediaSession.Feature.PLAY))
        assert(features.contains(MediaSession.Feature.PAUSE))
        assertFalse(features.contains(MediaSession.Feature.STOP))
        assertFalse(features.contains(MediaSession.Feature.SEEK_TO))
        assertFalse(features.contains(MediaSession.Feature.SEEK_FORWARD))
        assertFalse(features.contains(MediaSession.Feature.SEEK_BACKWARD))
        assertFalse(features.contains(MediaSession.Feature.SKIP_AD))
        assertFalse(features.contains(MediaSession.Feature.NEXT_TRACK))
        assertFalse(features.contains(MediaSession.Feature.PREVIOUS_TRACK))
        assertFalse(features.contains(MediaSession.Feature.FOCUS))

        features = MediaSession.Feature(MediaSession.Feature.STOP)
        assertEquals(features, MediaSession.Feature(MediaSession.Feature.STOP))
        assertEquals(features.hashCode(), MediaSession.Feature(MediaSession.Feature.STOP).hashCode())
    }

    @Test
    fun `media session controller interface works correctly`() {
        val fakeController = FakeController()
        assertFalse(fakeController.pause)
        assertFalse(fakeController.stop)
        assertFalse(fakeController.play)
        assertFalse(fakeController.seekTo)
        assertFalse(fakeController.seekForward)
        assertFalse(fakeController.seekBackward)
        assertFalse(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.pause()
        assert(fakeController.pause)
        assertFalse(fakeController.stop)
        assertFalse(fakeController.play)
        assertFalse(fakeController.seekTo)
        assertFalse(fakeController.seekForward)
        assertFalse(fakeController.seekBackward)
        assertFalse(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.stop()
        assert(fakeController.pause)
        assert(fakeController.stop)
        assertFalse(fakeController.play)
        assertFalse(fakeController.seekTo)
        assertFalse(fakeController.seekForward)
        assertFalse(fakeController.seekBackward)
        assertFalse(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.play()
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assertFalse(fakeController.seekTo)
        assertFalse(fakeController.seekForward)
        assertFalse(fakeController.seekBackward)
        assertFalse(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.seekTo(123.0, true)
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assert(fakeController.seekTo)
        assertFalse(fakeController.seekForward)
        assertFalse(fakeController.seekBackward)
        assertFalse(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.seekForward()
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assert(fakeController.seekTo)
        assert(fakeController.seekForward)
        assertFalse(fakeController.seekBackward)
        assertFalse(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.seekBackward()
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assert(fakeController.seekTo)
        assert(fakeController.seekForward)
        assert(fakeController.seekBackward)
        assertFalse(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.nextTrack()
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assert(fakeController.seekTo)
        assert(fakeController.seekForward)
        assert(fakeController.seekBackward)
        assert(fakeController.nextTrack)
        assertFalse(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.previousTrack()
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assert(fakeController.seekTo)
        assert(fakeController.seekForward)
        assert(fakeController.seekBackward)
        assert(fakeController.nextTrack)
        assert(fakeController.previousTrack)
        assertFalse(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.skipAd()
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assert(fakeController.seekTo)
        assert(fakeController.seekForward)
        assert(fakeController.seekBackward)
        assert(fakeController.nextTrack)
        assert(fakeController.previousTrack)
        assert(fakeController.skipAd)
        assertFalse(fakeController.muteAudio)

        fakeController.muteAudio(true)
        assert(fakeController.pause)
        assert(fakeController.stop)
        assert(fakeController.play)
        assert(fakeController.seekTo)
        assert(fakeController.seekForward)
        assert(fakeController.seekBackward)
        assert(fakeController.nextTrack)
        assert(fakeController.previousTrack)
        assert(fakeController.skipAd)
        assert(fakeController.muteAudio)
    }
}

private class FakeController : MediaSession.Controller {
    var pause = false
    var stop = false
    var play = false
    var seekTo = false
    var seekForward = false
    var seekBackward = false
    var nextTrack = false
    var previousTrack = false
    var skipAd = false
    var muteAudio = false

    override fun pause() {
        pause = true
    }

    override fun stop() {
        stop = true
    }

    override fun play() {
        play = true
    }

    override fun seekTo(time: Double, fast: Boolean) {
        seekTo = true
    }

    override fun seekForward() {
        seekForward = true
    }

    override fun seekBackward() {
        seekBackward = true
    }

    override fun nextTrack() {
        nextTrack = true
    }

    override fun previousTrack() {
        previousTrack = true
    }

    override fun skipAd() {
        skipAd = true
    }

    override fun muteAudio(mute: Boolean) {
        muteAudio = true
    }
}
