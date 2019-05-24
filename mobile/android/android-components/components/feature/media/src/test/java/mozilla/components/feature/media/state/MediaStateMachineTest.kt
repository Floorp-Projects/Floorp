/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.state

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.media.Media
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test

class MediaStateMachineTest {
    @Test
    fun `default state is None`() {
        val stateMachine = MediaStateMachine(SessionManager(mock()))
        assertEquals(MediaState.None, stateMachine.getState())
    }

    @Test
    fun `State changes when existing media session pauses and new media session starts playing`() {
        val sessionManager = SessionManager(mock())

        val initialMedia = MockMedia(Media.PlaybackState.PLAY)
        val initialSession = Session("https://www.mozilla.org").apply {
            sessionManager.add(this)
            media = listOf(initialMedia)
        }

        val stateMachine = MediaStateMachine(sessionManager)
        stateMachine.start()

        assertEquals(MediaState.Playing(initialSession, listOf(initialMedia)), stateMachine.getState())

        initialMedia.playbackState = Media.PlaybackState.PAUSE
        assertEquals(MediaState.Paused(initialSession, listOf(initialMedia)), stateMachine.getState())

        val newMedia = MockMedia(Media.PlaybackState.UNKNOWN)
        val newSession = Session("https://www.firefox.com").apply {
            sessionManager.add(this)
            media = listOf(newMedia)
        }

        assertEquals(MediaState.Paused(initialSession, listOf(initialMedia)), stateMachine.getState())

        newMedia.playbackState = Media.PlaybackState.WAITING
        assertEquals(MediaState.Paused(initialSession, listOf(initialMedia)), stateMachine.getState())

        newMedia.playbackState = Media.PlaybackState.PLAYING
        assertEquals(MediaState.Playing(newSession, listOf(newMedia)), stateMachine.getState())
    }

    @Test
    fun `State is updated after media is removed`() {
        val sessionManager = SessionManager(mock())

        val stateMachine = MediaStateMachine(sessionManager)
        stateMachine.start()

        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)
        }

        assertEquals(MediaState.None, stateMachine.getState())

        val media = MockMedia(Media.PlaybackState.PLAY)
        session.media = listOf(media)
        assertEquals(MediaState.Playing(session, listOf(media)), stateMachine.getState())

        session.media = emptyList()
        assertEquals(MediaState.None, stateMachine.getState())
    }

    @Test
    fun `State is updated after session is removed`() {
        val sessionManager = SessionManager(mock())

        val stateMachine = MediaStateMachine(sessionManager)
        stateMachine.start()

        assertEquals(MediaState.None, stateMachine.getState())

        val media = MockMedia(Media.PlaybackState.PLAYING)
        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)

            it.media = listOf(media)
        }

        assertEquals(MediaState.Playing(session, listOf(media)), stateMachine.getState())

        sessionManager.remove(session)
        assertEquals(MediaState.None, stateMachine.getState())
    }

    @Test
    fun `Multiple media of session start playing and stop`() {
        val sessionManager = SessionManager(mock())

        val stateMachine = MediaStateMachine(sessionManager)
        stateMachine.start()

        assertEquals(MediaState.None, stateMachine.getState())

        val media1 = MockMedia(Media.PlaybackState.PLAYING)
        val media2 = MockMedia(Media.PlaybackState.PAUSE)
        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)

            it.media = listOf(media1)
            it.media = listOf(media1, media2)
        }

        assertEquals(MediaState.Playing(session, listOf(media1)), stateMachine.getState())

        media2.playbackState = Media.PlaybackState.PLAYING

        assertEquals(MediaState.Playing(session, listOf(media1, media2)), stateMachine.getState())

        media1.playbackState = Media.PlaybackState.ENDED

        assertEquals(MediaState.Playing(session, listOf(media2)), stateMachine.getState())

        media2.playbackState = Media.PlaybackState.ENDED

        assertEquals(MediaState.None, stateMachine.getState())
    }

    @Test
    fun `Falls back to state NONE after stopping`() {
        val sessionManager = SessionManager(mock())

        val stateMachine = MediaStateMachine(sessionManager)
        stateMachine.start()

        val media = MockMedia(Media.PlaybackState.PLAYING)
        val session = Session("https://www.mozilla.org").also {
            sessionManager.add(it)

            it.media = listOf(media)
        }

        assertEquals(MediaState.Playing(session, listOf(media)), stateMachine.getState())

        media.playbackState = Media.PlaybackState.PAUSE
        assertEquals(MediaState.Paused(session, listOf(media)), stateMachine.getState())

        stateMachine.stop()
        assertEquals(MediaState.None, stateMachine.getState())

        // Does not change state since state machine is stopped
        media.playbackState = Media.PlaybackState.PLAYING
        assertEquals(MediaState.None, stateMachine.getState())
    }
}

class MockMedia(
    initialState: PlaybackState
) : Media() {
    init {
        playbackState = initialState
    }

    override val controller: Controller = mock()
}
