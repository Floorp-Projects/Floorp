/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import android.app.Notification
import android.content.BroadcastReceiver
import android.content.Intent
import android.support.v4.media.MediaMetadataCompat
import android.support.v4.media.session.PlaybackStateCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.concept.engine.mediasession.MediaSession.PlaybackState
import mozilla.components.feature.media.ext.toPlaybackState
import mozilla.components.feature.media.facts.MediaFacts
import mozilla.components.feature.media.session.MediaSessionCallback
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.coMock
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import android.media.session.PlaybackState as AndroidPlaybackState

@RunWith(AndroidJUnit4::class)
class MediaSessionServiceDelegateTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN the service is created THEN create a new notification scope audio focus manager`() {
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.mediaSession = mock()
        val mediaCallbackCaptor = argumentCaptor<MediaSessionCallback>()

        delegate.onCreate()

        verify(delegate.mediaSession).setCallback(mediaCallbackCaptor.capture())
        assertNotNull(delegate.notificationScope)
    }

    @Test
    fun `WHEN the service is destroyed THEN stop notification updates and abandon audio focus`() {
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.audioFocus = mock()

        delegate.onDestroy()

        verify(delegate.audioFocus)!!.abandon()
        verify(delegate.service, never()).stopSelf()
        assertNull(delegate.notificationScope)
    }

    @Test
    fun `GIVEN media playing started WHEN a new play command is received THEN resume media and emit telemetry`() {
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.controller = mock() // simulate media already started playing

        CollectionProcessor.withFactCollection { facts ->
            delegate.onStartCommand(Intent(AbstractMediaSessionService.ACTION_PLAY))

            verify(delegate.controller)!!.play()
            assertEquals(1, facts.size)
            with(facts[0]) {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.PLAY, action)
                assertEquals(MediaFacts.Items.NOTIFICATION, item)
            }
        }
    }

    @Test
    fun `GIVEN media playing started WHEN a new pause command is received THEN pause media and emit telemetry`() {
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.controller = mock() // simulate media already started playing

        CollectionProcessor.withFactCollection { facts ->
            delegate.onStartCommand(Intent(AbstractMediaSessionService.ACTION_PAUSE))

            verify(delegate.controller)!!.pause()
            assertEquals(1, facts.size)
            with(facts[0]) {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.PAUSE, action)
                assertEquals(MediaFacts.Items.NOTIFICATION, item)
            }
        }
    }

    @Test
    fun `WHEN the task is removed THEN stop media in all tabs and shutdown`() {
        val mediaTab1 = getMediaTab()
        val mediaTab2 = getMediaTab(PlaybackState.PAUSED)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(mediaTab1, mediaTab2),
            ),
        )
        val delegate = MediaSessionServiceDelegate(testContext, mock(), store)
        delegate.mediaSession = mock()

        delegate.onTaskRemoved()

        verify(mediaTab1.mediaSessionState!!.controller).stop()
        verify(mediaTab2.mediaSessionState!!.controller).stop()
        verify(delegate.mediaSession).release()
        verify(delegate.service).stopSelf()
    }

    @Test
    fun `WHEN handling playing media THEN emit telemetry`() {
        val mediaTab = getMediaTab()
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.audioFocus = mock()

        CollectionProcessor.withFactCollection { facts ->
            delegate.handleMediaPlaying(mediaTab)

            assertEquals(1, facts.size)
            with(facts[0]) {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.PLAY, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }
        }
    }

    @Test
    fun `WHEN handling playing media THEN setup internal properties`() {
        val mediaTab = getMediaTab()
        val delegate = spy(MediaSessionServiceDelegate(testContext, mock(), mock()))
        delegate.audioFocus = mock()

        delegate.handleMediaPlaying(mediaTab)

        verify(delegate).updateMediaSession(mediaTab)
        verify(delegate).registerBecomingNoisyListenerIfNeeded(mediaTab)
        assertSame(mediaTab.mediaSessionState!!.controller, delegate.controller)
    }

    @Test
    fun `GIVEN the service is already in foreground WHEN handling playing media THEN setup internal properties`() = runTestOnMain {
        val mediaTab = getMediaTab()
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.notificationManager = mock()
        delegate.onCreate()
        delegate.audioFocus = mock()
        delegate.isForegroundService = true

        delegate.handleMediaPlaying(mediaTab)

        verify(delegate.notificationManager).notify(eq(delegate.notificationId), any())
    }

    @Test
    fun `GIVEN the service is not in foreground WHEN handling playing media THEN start the media service as foreground`() {
        val mediaTab = getMediaTab()
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.onCreate()
        delegate.audioFocus = mock()
        delegate.isForegroundService = false

        delegate.handleMediaPlaying(mediaTab)

        verify(delegate.service).startForeground(eq(delegate.notificationId), any())
        assertTrue(delegate.isForegroundService)
    }

    @Test
    fun `WHEN updating the notification for a new media state THEN post a new notification`() = runTestOnMain {
        val mediaTab = getMediaTab()
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.notificationManager = mock()
        delegate.onCreate()
        val notification: Notification = mock()
        delegate.notificationHelper = coMock {
            doReturn(notification).`when`(this).create(mediaTab, delegate.mediaSession)
        }

        delegate.updateNotification(mediaTab)

        verify(delegate.notificationManager).notify(eq(delegate.notificationId), eq(notification))
    }

    @Test
    fun `WHEN starting the service as foreground THEN use start with a new notification for the current media state`() = runTestOnMain {
        val mediaTab = getMediaTab()
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.onCreate()
        val notification: Notification = mock()
        delegate.notificationHelper = coMock {
            doReturn(notification).`when`(this).create(mediaTab, delegate.mediaSession)
        }

        delegate.startForeground(mediaTab)

        verify(delegate.service).startForeground(eq(delegate.notificationId), eq(notification))
        assertTrue(delegate.isForegroundService)
    }

    @Test
    fun `GIVEN media is paused WHEN media is handling resuming media THEN resume the right session`() {
        val mediaTab1 = getMediaTab()
        val mediaTab2 = getMediaTab(PlaybackState.PAUSED)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(mediaTab1, mediaTab2),
            ),
        )
        val service: AbstractMediaSessionService = mock()
        val delegate = MediaSessionServiceDelegate(testContext, service, store)
        val mediaSessionCallback = MediaSessionCallback(store)
        delegate.onCreate()

        mediaSessionCallback.onPause()
        verify(mediaTab1.mediaSessionState!!.controller).pause()

        mediaSessionCallback.onPlay()
        verify(mediaTab1.mediaSessionState!!.controller).play()
    }

    @Test
    fun `WHEN handling paused media THEN emit telemetry`() {
        val mediaTab = getMediaTab(PlaybackState.PAUSED)
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())

        CollectionProcessor.withFactCollection { facts ->
            delegate.handleMediaPaused(mediaTab)

            assertEquals(1, facts.size)
            with(facts[0]) {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.PAUSE, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }
        }
    }

    @Test
    fun `WHEN handling paused media THEN update internal state and notification and stop the service`() = runTestOnMain {
        val mediaTab = getMediaTab(PlaybackState.PAUSED)
        val delegate = spy(MediaSessionServiceDelegate(testContext, mock(), mock()))
        delegate.notificationManager = mock()
        delegate.isForegroundService = true
        delegate.onCreate()

        delegate.handleMediaPaused(mediaTab)

        verify(delegate).updateMediaSession(mediaTab)
        verify(delegate).unregisterBecomingNoisyListenerIfNeeded()
        verify(delegate.service).stopForeground(false)
        verify(delegate.notificationManager).notify(eq(delegate.notificationId), any())
        assertFalse(delegate.isForegroundService)
    }

    @Test
    fun `WHEN handling stopped media THEN emit telemetry`() {
        val mediaTab = getMediaTab(PlaybackState.STOPPED)
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())

        CollectionProcessor.withFactCollection { facts ->
            delegate.handleMediaStopped(mediaTab)

            assertEquals(1, facts.size)
            with(facts[0]) {
                assertEquals(Component.FEATURE_MEDIA, component)
                assertEquals(Action.STOP, action)
                assertEquals(MediaFacts.Items.STATE, item)
            }
        }
    }

    @Test
    fun `WHEN handling stopped media THEN update internal state and notification and stop the service`() = runTestOnMain {
        val mediaTab = getMediaTab(PlaybackState.STOPPED)
        val delegate = spy(MediaSessionServiceDelegate(testContext, mock(), mock()))
        delegate.notificationManager = mock()
        delegate.isForegroundService = true
        delegate.onCreate()

        delegate.handleMediaStopped(mediaTab)

        verify(delegate).updateMediaSession(mediaTab)
        verify(delegate).unregisterBecomingNoisyListenerIfNeeded()
        verify(delegate.service).stopForeground(false)
        verify(delegate.notificationManager).notify(eq(delegate.notificationId), any())
        assertFalse(delegate.isForegroundService)
    }

    @Test
    fun `WHEN there is no media playing THEN stop the media service`() {
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.audioFocus = mock()
        delegate.mediaSession = mock()

        delegate.handleNoMedia()

        verify(delegate.mediaSession).release()
        verify(delegate.service).stopSelf()
    }

    @Test
    fun `WHEN updating the media session THEN use the values from the current media session`() {
        val mediaTab = getMediaTab()
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.mediaSession = mock()
        val metadataCaptor = argumentCaptor<MediaMetadataCompat>()
        // Need to capture method arguments and manually check for equality
        val playbackStateCaptor = argumentCaptor<PlaybackStateCompat>()
        val expectedPlaybackState = mediaTab.mediaSessionState!!.toPlaybackState()

        delegate.updateMediaSession(mediaTab)

        verify(delegate.mediaSession).isActive = true
        verify(delegate.mediaSession).setPlaybackState(playbackStateCaptor.capture())
        assertEquals(expectedPlaybackState.state, playbackStateCaptor.value.state)
        assertEquals(
            (expectedPlaybackState.playbackState as AndroidPlaybackState).state,
            (playbackStateCaptor.value.playbackState as AndroidPlaybackState).state,
        )
        assertEquals(
            (expectedPlaybackState.playbackState as AndroidPlaybackState).position,
            (playbackStateCaptor.value.playbackState as AndroidPlaybackState).position,
        )
        assertEquals(
            (expectedPlaybackState.playbackState as AndroidPlaybackState).playbackSpeed,
            (playbackStateCaptor.value.playbackState as AndroidPlaybackState).playbackSpeed,
        )
        assertEquals(
            (expectedPlaybackState.playbackState as AndroidPlaybackState).actions,
            (playbackStateCaptor.value.playbackState as AndroidPlaybackState).actions,
        )
        assertEquals(
            (expectedPlaybackState.playbackState as AndroidPlaybackState).customActions,
            (playbackStateCaptor.value.playbackState as AndroidPlaybackState).customActions,
        )
        assertEquals(expectedPlaybackState.playbackSpeed, playbackStateCaptor.value.playbackSpeed)
        assertEquals(expectedPlaybackState.actions, playbackStateCaptor.value.actions)
        assertEquals(expectedPlaybackState.position, playbackStateCaptor.value.position)
        verify(delegate.mediaSession).setMetadata(metadataCaptor.capture())
        assertEquals(mediaTab.content.title, metadataCaptor.value.bundle[MediaMetadataCompat.METADATA_KEY_TITLE])
        assertEquals(mediaTab.content.url, metadataCaptor.value.bundle[MediaMetadataCompat.METADATA_KEY_ARTIST])
        assertEquals(-1L, metadataCaptor.value.bundle[MediaMetadataCompat.METADATA_KEY_DURATION])
    }

    @Test
    fun `WHEN stopping running in foreground THEN stop the foreground service`() {
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.isForegroundService = true

        delegate.stopForeground()

        verify(delegate.service).stopForeground(false)
        assertFalse(delegate.isForegroundService)
    }

    @Test
    fun `GIVEN a audio noisy receiver is already registered WHEN trying to register a new one THEN return early`() {
        val context = spy(testContext)
        val delegate = MediaSessionServiceDelegate(context, mock(), mock())
        delegate.noisyAudioStreamReceiver = mock()

        delegate.registerBecomingNoisyListenerIfNeeded(mock())

        verify(context, never()).registerReceiver(any(), any())
    }

    @Test
    fun `GIVEN a audio noisy receiver is not already registered WHEN trying to register a new one THEN register it`() {
        val context = spy(testContext)
        val delegate = MediaSessionServiceDelegate(context, mock(), mock())
        val receiverCaptor = argumentCaptor<BroadcastReceiver>()

        delegate.registerBecomingNoisyListenerIfNeeded(mock())

        verify(context).registerReceiver(receiverCaptor.capture(), eq(delegate.intentFilter))
        assertEquals(BecomingNoisyReceiver::class.java, receiverCaptor.value.javaClass)
    }

    @Test
    fun `GIVEN a audio noisy receiver is already registered WHEN trying to unregister one THEN unregister it`() {
        val context = spy(testContext)
        val delegate = MediaSessionServiceDelegate(context, mock(), mock())
        delegate.noisyAudioStreamReceiver = mock()
        context.registerReceiver(delegate.noisyAudioStreamReceiver, delegate.intentFilter)
        val receiverCaptor = argumentCaptor<BroadcastReceiver>()

        delegate.unregisterBecomingNoisyListenerIfNeeded()

        verify(context).unregisterReceiver(receiverCaptor.capture())
        assertEquals(BecomingNoisyReceiver::class.java, receiverCaptor.value.javaClass)
        assertNull(delegate.noisyAudioStreamReceiver)
    }

    @Test
    fun `GIVEN a audio noisy receiver is not already registered WHEN trying to unregister one THEN return early`() {
        val context = spy(testContext)
        val delegate = MediaSessionServiceDelegate(context, mock(), mock())

        delegate.unregisterBecomingNoisyListenerIfNeeded()

        verify(context, never()).unregisterReceiver(any())
    }

    @Test
    fun `WHEN the delegate is shutdown THEN cleanup resources and stop the media service`() {
        val delegate = MediaSessionServiceDelegate(testContext, mock(), mock())
        delegate.mediaSession = mock()

        delegate.shutdown()

        verify(delegate.mediaSession).release()
        verify(delegate.service).stopSelf()
    }

    @Test
    fun `when device is becoming noisy, playback is paused`() {
        val controller: MediaSession.Controller = mock()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(controller, playbackState = PlaybackState.PLAYING),
                ),
            ),
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = MediaSessionServiceDelegate(testContext, service, store)
        delegate.onCreate()
        delegate.handleMediaPlaying(initialState.tabs[0])

        delegate.deviceBecomingNoisy(testContext)

        verify(controller).pause()
    }

    private fun getMediaTab(playbackState: PlaybackState = PlaybackState.PLAYING) = createTab(
        title = "Mozilla",
        url = "https://www.mozilla.org",
        mediaSessionState = MediaSessionState(mock(), playbackState = playbackState),
    )
}
