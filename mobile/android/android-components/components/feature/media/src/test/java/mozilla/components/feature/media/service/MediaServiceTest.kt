/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.MediaFeature
import mozilla.components.feature.media.MockMedia
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric

@RunWith(AndroidJUnit4::class)
class MediaServiceTest {
    @Before
    @After
    fun setUp() {
        MediaStateMachine.stop()
    }

    @Test
    fun `Playing state starts service in foreground`() {
        val media = MockMedia(Media.PlaybackState.UNKNOWN)

        val sessionManager = SessionManager(engine = mock()).apply {
            add(Session("https://www.mozilla.org").also { it.media = listOf(media) })
        }

        MediaStateMachine.start(sessionManager)

        val feature = MediaFeature(mock())
        feature.enable()

        media.playbackState = Media.PlaybackState.PLAYING

        val service = spy(Robolectric.buildService(MediaService::class.java)
            .create()
            .get())

        service.onStartCommand(MediaService.updateStateIntent(testContext), 0, 0)

        verify(service).startForeground(ArgumentMatchers.anyInt(), any())
    }

    @Test
    fun `Switching from playing to none stops service`() {
        val media = MockMedia(Media.PlaybackState.UNKNOWN)

        val sessionManager = SessionManager(engine = mock()).apply {
            add(Session("https://www.mozilla.org").also { it.media = listOf(media) })
        }

        MediaStateMachine.start(sessionManager)

        val feature = MediaFeature(mock())
        feature.enable()

        media.playbackState = Media.PlaybackState.PLAYING

        val service = spy(Robolectric.buildService(MediaService::class.java)
            .create()
            .get())

        service.onStartCommand(MediaService.updateStateIntent(testContext), 0, 0)

        verify(service, never()).stopSelf()

        media.playbackState = Media.PlaybackState.ENDED

        service.onStartCommand(MediaService.updateStateIntent(testContext), 0, 0)

        verify(service).stopSelf()
    }

    @Test
    fun `Switching from playing to pause stops serving from being in the foreground`() {
        val media = MockMedia(Media.PlaybackState.UNKNOWN)

        val sessionManager = SessionManager(engine = mock()).apply {
            add(Session("https://www.mozilla.org").also { it.media = listOf(media) })
        }

        MediaStateMachine.start(sessionManager)

        val feature = MediaFeature(mock())
        feature.enable()

        media.playbackState = Media.PlaybackState.PLAYING

        val service = spy(Robolectric.buildService(MediaService::class.java)
            .create()
            .get())

        verify(service, never()).startForeground(anyInt(), any())

        service.onStartCommand(MediaService.updateStateIntent(testContext), 0, 0)

        verify(service).startForeground(anyInt(), any())

        media.playbackState = Media.PlaybackState.PAUSE

        verify(service, never()).stopForeground(false)

        service.onStartCommand(MediaService.updateStateIntent(testContext), 0, 0)

        verify(service).stopForeground(false)
    }
}
