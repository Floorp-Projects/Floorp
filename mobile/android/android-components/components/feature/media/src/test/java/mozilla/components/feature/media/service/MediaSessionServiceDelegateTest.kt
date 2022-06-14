/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.session.MediaSessionCallback
import mozilla.components.support.test.any
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MediaSessionServiceDelegateTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `media session state starts service in foreground`() {
        val initialState = BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", mediaSessionState = MediaSessionState(mock())))
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = MediaSessionServiceDelegate(testContext, service, store)

        delegate.onCreate()
        delegate.onStartCommand(
            AbstractMediaSessionService.launchIntent(
                testContext,
                service::class.java
            )
        )

        verify(service).startForeground(ArgumentMatchers.anyInt(), any())
        assertTrue(delegate.isForegroundService)

        delegate.onDestroy()
    }

    @Test
    fun `deactivating media session stops service`() {
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PLAYING)
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))

        delegate.onCreate()

        verify(service).startForeground(ArgumentMatchers.anyInt(), any())
        verify(service, never()).stopSelf()
        assertTrue(delegate.isForegroundService)

        store.dispatch(MediaSessionAction.DeactivatedMediaSessionAction(store.state.tabs[0].id))

        store.waitUntilIdle()

        verify(service).stopSelf()
        verify(delegate).shutdown()

        delegate.onDestroy()
    }

    @Test
    fun `deactivating media session in custom tab stops service`() {
        val initialState = BrowserState(
            customTabs = listOf(
                createCustomTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PLAYING)
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))

        delegate.onCreate()

        verify(service).startForeground(ArgumentMatchers.anyInt(), any())
        verify(service, never()).stopSelf()
        verify(delegate, never()).shutdown()
        assertTrue(delegate.isForegroundService)

        store.dispatch(MediaSessionAction.DeactivatedMediaSessionAction(store.state.customTabs[0].id))

        store.waitUntilIdle()

        verify(service).stopSelf()
        verify(delegate).shutdown()

        delegate.onDestroy()
    }

    @Test
    fun `launch intent will always call startForeground`() {
        val controller: MediaSession.Controller = mock()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        controller,
                        playbackState = MediaSession.PlaybackState.PLAYING
                    )
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))

        delegate.onCreate()

        verify(service, times(1)).startForeground(ArgumentMatchers.anyInt(), any())
        verify(service, never()).stopSelf()
        verify(delegate, never()).shutdown()
        verify(controller, never()).pause()
        assertTrue(delegate.isForegroundService)

        delegate.onStartCommand(
            AbstractMediaSessionService.launchIntent(
                testContext,
                service::class.java
            )
        )
        verify(service, times(2)).startForeground(ArgumentMatchers.anyInt(), any())

        delegate.onStartCommand(
            AbstractMediaSessionService.launchIntent(
                testContext,
                service::class.java
            )
        )
        verify(service, times(3)).startForeground(ArgumentMatchers.anyInt(), any())
    }

    @Test
    fun `pause intent will pause playing media session`() {
        val controller: MediaSession.Controller = mock()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        controller,
                        playbackState = MediaSession.PlaybackState.PLAYING
                    )
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))

        delegate.onCreate()

        verify(service).startForeground(ArgumentMatchers.anyInt(), any())
        verify(service, never()).stopSelf()
        verify(delegate, never()).shutdown()
        verify(controller, never()).pause()
        assertTrue(delegate.isForegroundService)

        delegate.onStartCommand(
            AbstractMediaSessionService.pauseIntent(
                testContext,
                service::class.java
            )
        )

        verify(controller).pause()
    }

    @Test
    fun `play intent will play paused media session`() {
        val controller: MediaSession.Controller = mock()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        controller,
                        playbackState = MediaSession.PlaybackState.PAUSED
                    )
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))

        delegate.onCreate()

        verify(service, never()).stopSelf()
        verify(delegate, never()).shutdown()
        verify(controller, never()).pause()
        verify(service).stopForeground(false)
        assertFalse(delegate.isForegroundService)

        delegate.onStartCommand(
            AbstractMediaSessionService.playIntent(
                testContext,
                service::class.java
            )
        )

        verify(controller).play()
    }

    @Test
    fun `Media session callback will pause and play the session`() {
        val controller: MediaSession.Controller = mock()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        controller,
                        playbackState = MediaSession.PlaybackState.PLAYING
                    )
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = MediaSessionServiceDelegate(testContext, service, store)
        val mediaSessionCallback = MediaSessionCallback(store)

        delegate.onCreate()

        verify(service, never()).stopSelf()
        verify(controller, never()).pause()
        assertTrue(delegate.isForegroundService)
        verify(service, times(1)).startForeground(ArgumentMatchers.anyInt(), any())

        delegate.onStartCommand(
            AbstractMediaSessionService.launchIntent(
                testContext,
                service::class.java
            )
        )

        verify(service, times(2)).startForeground(ArgumentMatchers.anyInt(), any())

        delegate.onStartCommand(
            AbstractMediaSessionService.pauseIntent(
                testContext,
                service::class.java
            )
        )
        verify(controller).pause()
        mediaSessionCallback.onPause()
        verify(service, times(2)).startForeground(ArgumentMatchers.anyInt(), any())

        mediaSessionCallback.onPlay()
        verify(controller, times(1)).play()
    }

    @Test
    fun `Media session callback will resume the right session`() {
        val controller: MediaSession.Controller = mock()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        controller,
                        playbackState = MediaSession.PlaybackState.PLAYING
                    )
                ),
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        playbackState = MediaSession.PlaybackState.PAUSED
                    )
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))
        val mediaSessionCallback = MediaSessionCallback(store)

        delegate.onCreate()

        verify(service).startForeground(ArgumentMatchers.anyInt(), any())
        verify(service, never()).stopSelf()
        verify(delegate, never()).shutdown()
        verify(controller, never()).pause()
        verify(controller, never()).play()
        assertTrue(delegate.isForegroundService)

        mediaSessionCallback.onPause()
        verify(controller).pause()

        mediaSessionCallback.onPlay()
        verify(controller).play()
    }

    @Test
    fun `destroying service will stop all playback sessions`() {
        val controller: MediaSession.Controller = mock()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        controller,
                        playbackState = MediaSession.PlaybackState.PLAYING
                    )
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))

        delegate.onCreate()

        delegate.onDestroy()
        verify(service, never()).stopSelf()
        verify(delegate).destroy()
    }

    @Test
    fun `when device is becoming noisy, playback is paused`() {
        val controller: MediaSession.Controller = mock()

        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(controller, playbackState = MediaSession.PlaybackState.PLAYING)
                )
            )
        )
        val store = BrowserStore(initialState)
        val service: AbstractMediaSessionService = mock()
        val delegate = spy(MediaSessionServiceDelegate(testContext, service, store))

        delegate.onCreate()

        verify(service, never()).stopSelf()
        verify(delegate, never()).shutdown()

        delegate.deviceBecomingNoisy(testContext)

        verify(controller).pause()
    }
}
