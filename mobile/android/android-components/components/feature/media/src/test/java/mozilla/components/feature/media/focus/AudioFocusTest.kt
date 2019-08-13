/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.focus

import android.media.AudioManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.MockMedia
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class AudioFocusTest {
    private lateinit var audioManager: AudioManager

    @Before
    fun setUp() {
        audioManager = mock()
    }

    @Test
    fun `Successful request will not change media in state`() {
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
            .`when`(audioManager).requestAudioFocus(any())

        val state = spy(MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(MockMedia(Media.PlaybackState.PLAYING))))

        val audioFocus = AudioFocus(audioManager)
        audioFocus.request(state)

        verify(audioManager).requestAudioFocus(any())
        verifyNoMoreInteractions(state)
    }

    @Test
    fun `Failed request will pause media`() {
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_FAILED)
            .`when`(audioManager).requestAudioFocus(any())

        val media = MockMedia(Media.PlaybackState.PLAYING)

        val state = spy(MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(media)))

        val audioFocus = AudioFocus(audioManager)
        audioFocus.request(state)

        verify(audioManager).requestAudioFocus(any())
        verify(media.controller).pause()
    }

    @Test
    fun `Delayed request will pause media`() {
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_DELAYED)
            .`when`(audioManager).requestAudioFocus(any())

        val media = MockMedia(Media.PlaybackState.PLAYING)

        val state = spy(MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(media)))

        val audioFocus = AudioFocus(audioManager)
        audioFocus.request(state)

        verify(audioManager).requestAudioFocus(any())
        verify(media.controller).pause()
    }

    @Test
    fun `Will pause and resume on and after transient focus loss`() {
        val media = MockMedia(Media.PlaybackState.PLAYING)

        doReturn(AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
            .`when`(audioManager).requestAudioFocus(any())

        val audioFocus = AudioFocus(audioManager)

        verifyNoMoreInteractions(media.controller)

        MediaStateMachine.state = MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(media))

        audioFocus.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT)

        verify(media.controller).pause()
        verifyNoMoreInteractions(media.controller)

        MediaStateMachine.state = MediaState.Paused(
            Session("https://www.mozilla.org"),
            listOf(media))

        audioFocus.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN)

        verify(media.controller).play()
        verifyNoMoreInteractions(media.controller)
    }

    @Test
    fun `Will resume playback when gaining focus after being delayed`() {
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_DELAYED)
            .`when`(audioManager).requestAudioFocus(any())

        val media = MockMedia(Media.PlaybackState.PLAYING)

        val audioFocus = AudioFocus(audioManager)
        audioFocus.request(MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(media)))

        verify(media.controller).pause()

        MediaStateMachine.state = MediaState.Paused(
                Session("https://www.mozilla.org"),
                listOf(media))

        audioFocus.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN)

        verify(media.controller).play()
    }

    @Test(expected = IllegalStateException::class)
    fun `An unknown audio focus response will throw an exception`() {
        doReturn(999)
            .`when`(audioManager).requestAudioFocus(any())

        val state = spy(MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(MockMedia(Media.PlaybackState.PLAYING))))

        val audioFocus = AudioFocus(audioManager)
        audioFocus.request(state)
    }

    @Test
    fun `An unknown focus change event will be ignored`() {
        val media = MockMedia(Media.PlaybackState.PLAYING)

        doReturn(AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
            .`when`(audioManager).requestAudioFocus(any())

        val audioFocus = AudioFocus(audioManager)

        verifyNoMoreInteractions(media.controller)

        MediaStateMachine.state = MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(media))

        audioFocus.onAudioFocusChange(999)
    }

    @Test
    fun `An audio focus loss will pause media and regain will not resume automatically`() {
        doReturn(AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
            .`when`(audioManager).requestAudioFocus(any())

        val media = MockMedia(Media.PlaybackState.PLAYING)

        val audioFocus = AudioFocus(audioManager)
        audioFocus.request(MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(media)))

        MediaStateMachine.state = MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(media))

        verifyNoMoreInteractions(media.controller)

        audioFocus.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS)

        verify(media.controller).pause()

        MediaStateMachine.state = MediaState.Paused(
            Session("https://www.mozilla.org"),
            listOf(media))

        audioFocus.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN)

        verifyNoMoreInteractions(media.controller)
    }
}