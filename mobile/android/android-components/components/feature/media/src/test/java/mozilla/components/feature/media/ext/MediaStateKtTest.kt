/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.ext

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.MockMedia
import mozilla.components.feature.media.state.MediaState
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class MediaStateKtTest {
    @Test
    fun `getSession() extension method`() {
        val noneState = MediaState.None
        assertNull(noneState.getSession())

        val playingState = MediaState.Playing(
            Session(id = "test1", initialUrl = "https://www.mozilla.org"),
            emptyList())
        assertNotNull(playingState.getSession())
        assertEquals("test1", playingState.getSession()!!.id)

        val pausedState = MediaState.Paused(
            Session(id = "test2", initialUrl = "https://www.mozilla.org"),
            emptyList())
        assertNotNull(pausedState.getSession())
        assertEquals("test2", pausedState.getSession()!!.id)
    }

    @Test
    fun `pauseIfPlaying() extension method`() {
        val noneState = MediaState.None
        noneState.pauseIfPlaying() // Does nothing and has no media -> nothing to verify/assert.

        val playingMedia: Media = MockMedia(Media.PlaybackState.PLAYING)
        val playingState = MediaState.Playing(session = mock(), media = listOf(playingMedia))
        playingState.pauseIfPlaying()
        verify(playingMedia.controller).pause()

        val pausedMedia: Media = MockMedia(Media.PlaybackState.PAUSE)
        val pausedState = MediaState.Paused(session = mock(), media = listOf(pausedMedia))
        pausedState.pauseIfPlaying()
        verify(pausedMedia.controller, never()).pause()
    }

    @Test
    fun `playIfPaused() extension method`() {
        val noneState = MediaState.None
        noneState.playIfPaused() // Does nothing and has no media -> nothing to verify/assert.

        val playingMedia: Media = MockMedia(Media.PlaybackState.PLAYING)
        val playingState = MediaState.Playing(session = mock(), media = listOf(playingMedia))
        playingState.playIfPaused()
        verify(playingMedia.controller, never()).play()

        val pausedMedia: Media = MockMedia(Media.PlaybackState.PAUSE)
        val pausedState = MediaState.Paused(session = mock(), media = listOf(pausedMedia))
        pausedState.playIfPaused()
        verify(pausedMedia.controller).play()
    }

    @Test
    fun `isForCustomTabSession() extension method`() {
        assertFalse(
            MediaState.Playing(
                Session("https://www.mozilla.org"), media = listOf(mock())
            ).isForCustomTabSession()
        )

        assertTrue(
            MediaState.Playing(
                Session("https://www.mozilla.org").also {
                    it.customTabConfig = mock()
                }, media = listOf(mock())
            ).isForCustomTabSession()
        )

        assertFalse(
            MediaState.Paused(
                Session("https://www.mozilla.org"), media = listOf(mock())
            ).isForCustomTabSession()
        )

        assertTrue(
            MediaState.Paused(
                Session("https://www.mozilla.org").also {
                    it.customTabConfig = mock()
                }, media = listOf(mock())
            ).isForCustomTabSession()
        )

        assertFalse(
            MediaState.None.isForCustomTabSession()
        )
    }
}
