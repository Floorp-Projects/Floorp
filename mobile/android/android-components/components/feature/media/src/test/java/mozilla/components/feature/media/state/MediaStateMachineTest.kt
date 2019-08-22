/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.state

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.MockMedia
import mozilla.components.feature.media.ext.getMedia
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class MediaStateMachineTest {
    @Before
    @After
    fun setUp() {
        MediaStateMachine.stop()
    }

    @Test
    fun `default state is None`() {
        assertEquals(MediaState.None, MediaStateMachine.state)
    }

    @Test
    fun `State changes when existing media session pauses and new media session starts playing`() {
        val sessionManager = SessionManager(mock())

        val initialMedia = MockMedia(Media.PlaybackState.PLAY)
        val initialSession = Session("https://www.mozilla.org").apply {
            sessionManager.add(this)
            media = listOf(initialMedia)
        }

        MediaStateMachine.start(sessionManager)
        MediaStateMachine.waitForStateChange()

        assertEquals(MediaState.Playing(initialSession, listOf(initialMedia)), MediaStateMachine.state)

        initialMedia.playbackState = Media.PlaybackState.PAUSE
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Paused(initialSession, listOf(initialMedia)), MediaStateMachine.state)

        val newMedia = MockMedia(Media.PlaybackState.UNKNOWN)
        val newSession = Session("https://www.firefox.com").apply {
            sessionManager.add(this)
            media = listOf(newMedia)
        }
        MediaStateMachine.waitForStateChange()

        assertEquals(MediaState.Paused(initialSession, listOf(initialMedia)), MediaStateMachine.state)

        newMedia.playbackState = Media.PlaybackState.WAITING
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Paused(initialSession, listOf(initialMedia)), MediaStateMachine.state)

        newMedia.playbackState = Media.PlaybackState.PLAYING
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Playing(newSession, listOf(newMedia)), MediaStateMachine.state)
    }

    @Test
    fun `State is updated after media is removed`() {
        val sessionManager = SessionManager(mock())

        MediaStateMachine.start(sessionManager)

        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)
        }

        MediaStateMachine.waitForStateChange()

        assertEquals(MediaState.None, MediaStateMachine.state)

        val media = MockMedia(Media.PlaybackState.PLAY)
        session.media = listOf(media)
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Playing(session, listOf(media)), MediaStateMachine.state)

        session.media = emptyList()
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.None, MediaStateMachine.state)
    }

    @Test
    fun `State is updated after session is removed`() {
        val sessionManager = SessionManager(mock())

        MediaStateMachine.start(sessionManager)

        assertEquals(MediaState.None, MediaStateMachine.state)

        val media = MockMedia(Media.PlaybackState.PLAYING)
        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)

            it.media = listOf(media)
        }

        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Playing(session, listOf(media)), MediaStateMachine.state)

        sessionManager.remove(session)
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.None, MediaStateMachine.state)
    }

    @Test
    fun `Multiple media of session start playing and stop`() {
        val sessionManager = SessionManager(mock())

        MediaStateMachine.start(sessionManager)

        assertEquals(MediaState.None, MediaStateMachine.state)

        val media1 = MockMedia(Media.PlaybackState.PLAYING)
        val media2 = MockMedia(Media.PlaybackState.PAUSE)
        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)

            it.media = listOf(media1)
            it.media = listOf(media1, media2)
        }

        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Playing(session, listOf(media1)), MediaStateMachine.state)

        media2.playbackState = Media.PlaybackState.PLAYING

        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Playing(session, listOf(media1, media2)), MediaStateMachine.state)

        media1.playbackState = Media.PlaybackState.ENDED

        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Playing(session, listOf(media2)), MediaStateMachine.state)

        media2.playbackState = Media.PlaybackState.ENDED

        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.None, MediaStateMachine.state)
    }

    @Test
    fun `Falls back to state NONE after stopping`() {
        val sessionManager = SessionManager(mock())

        MediaStateMachine.start(sessionManager)

        val media = MockMedia(Media.PlaybackState.PLAYING)
        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)

            it.media = listOf(media)
        }

        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Playing(session, listOf(media)), MediaStateMachine.state)

        media.playbackState = Media.PlaybackState.PAUSE
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.Paused(session, listOf(media)), MediaStateMachine.state)

        MediaStateMachine.stop()
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.None, MediaStateMachine.state)

        // Does not change state since state machine is stopped
        media.playbackState = Media.PlaybackState.PLAYING
        MediaStateMachine.waitForStateChange()
        assertEquals(MediaState.None, MediaStateMachine.state)
    }

    @Test
    fun `Only media that was playing is added to paused state`() {
        val media1 = MockMedia(Media.PlaybackState.PLAYING)
        val media2 = MockMedia(Media.PlaybackState.PAUSE)

        val sessionManager = SessionManager(mock())

        Session("https://www.mozilla.org").also {
            sessionManager.add(it)

            it.media = listOf(media1)
            it.media = listOf(media1, media2)
        }

        MediaStateMachine.start(sessionManager)

        MediaStateMachine.waitForStateChange()
        MediaStateMachine.state.also {
            assertTrue(it is MediaState.Playing)
            assertEquals(1, it.getMedia().size)
            assertEquals(media1, it.getMedia()[0])
        }

        media1.playbackState = Media.PlaybackState.PAUSE

        MediaStateMachine.waitForStateChange()
        MediaStateMachine.state.also {
            assertTrue(it is MediaState.Paused)
            assertEquals(1, it.getMedia().size)
            assertEquals(media1, it.getMedia()[0])
        }
    }
}
