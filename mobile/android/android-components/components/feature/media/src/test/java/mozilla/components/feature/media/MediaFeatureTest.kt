/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MediaFeatureTest {
    @Before
    @After
    fun setUp() {
        MediaStateMachine.stop()
    }

    @Test
    fun `Media playing in Session starts service`() {
        val context: Context = mock()
        val sessionManager = SessionManager(engine = mock())

        MediaStateMachine.start(sessionManager)

        val feature = MediaFeature(context)
        feature.enable()

        // A session gets added
        val session = Session("https://www.mozilla.org")
        sessionManager.add(session)

        // A media object gets added to the session
        val media = MockMedia(Media.PlaybackState.UNKNOWN)
        doReturn(30.0).`when`(media.metadata).duration
        session.media = listOf(media)

        media.playbackState = Media.PlaybackState.WAITING

        // So far nothing has happened yet
        verify(context, never()).startService(any())

        // Media starts playing!
        media.playbackState = Media.PlaybackState.PLAYING

        verify(context).startService(any())
    }

    @Test
    fun `Media with short duration will not start service`() {
        val context: Context = mock()
        val sessionManager = SessionManager(engine = mock())

        MediaStateMachine.start(sessionManager)

        val feature = MediaFeature(context)
        feature.enable()

        // A session gets added
        val session = Session("https://www.mozilla.org")
        sessionManager.add(session)

        // A media object gets added to the session
        val media = MockMedia(Media.PlaybackState.UNKNOWN)
        doReturn(2.0).`when`(media.metadata).duration
        session.media = listOf(media)

        media.playbackState = Media.PlaybackState.WAITING

        // So far nothing has happened yet
        verify(context, never()).startService(any())

        // Media starts playing!
        media.playbackState = Media.PlaybackState.PLAYING

        // Service still not started since duration is too short
        verify(context, never()).startService(any())
    }

    @Test
    fun `Media switching from playing to pause send Intent to service`() {
        val context: Context = mock()
        val media = MockMedia(Media.PlaybackState.PLAYING)
        doReturn(30.0).`when`(media.metadata).duration

        val sessionManager = SessionManager(engine = mock()).apply {
            add(Session("https://www.mozilla.org").also { it.media = listOf(media) })
        }

        MediaStateMachine.start(sessionManager)

        val feature = MediaFeature(context)
        feature.enable()

        reset(context)
        verify(context, never()).startService(any())

        media.playbackState = Media.PlaybackState.PAUSE

        verify(context).startService(any())
    }

    @Test
    fun `Media stopping to play will notify service`() {
        val context: Context = mock()
        val media = MockMedia(Media.PlaybackState.UNKNOWN)
        doReturn(30.0).`when`(media.metadata).duration

        val sessionManager = SessionManager(engine = mock()).apply {
            add(Session("https://www.mozilla.org").also { it.media = listOf(media) })
        }

        MediaStateMachine.start(sessionManager)

        val feature = MediaFeature(context)
        feature.enable()

        media.playbackState = Media.PlaybackState.PLAYING

        verify(context).startService(any())

        reset(context)
        verify(context, never()).startService(any())

        media.playbackState = Media.PlaybackState.ENDED

        verify(context).startService(any())
    }
}
