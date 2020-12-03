/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.service.MediaSessionServiceDelegate
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MediaSessionFeatureTest {
    private val dispatcher: TestCoroutineDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(dispatcher)

    @Test
    fun `feature triggers foreground service when there's is media session state`() {
        val mockApplicationContext: Context = mock()
        val initialState = BrowserState(
            tabs = listOf(createTab(
                "https://www.mozilla.org",
                mediaSessionState = MediaSessionState(
                    mock(),
                    playbackState = MediaSession.PlaybackState.PLAYING
                )
            ))
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFeature(
            mockApplicationContext,
            MediaSessionServiceDelegate::class.java,
            store
        )

        feature.start()

        verify(mockApplicationContext).startForegroundService(any())
    }

    @Test
    fun `no media session states will not trigger foreground service`() {
        val mockApplicationContext: Context = mock()
        val initialState = BrowserState(
            tabs = listOf(createTab(
                "https://www.mozilla.org",
                mediaSessionState = null
            ))
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFeature(
            mockApplicationContext,
            MediaSessionServiceDelegate::class.java,
            store
        )

        feature.start()

        verify(mockApplicationContext, never()).startForegroundService(any())
    }

    @Test
    fun `feature observes media session state changes`() {
        val mockApplicationContext: Context = mock()
        val initialState = BrowserState(
            tabs = listOf(createTab(
                "https://www.mozilla.org",
                mediaSessionState = null
            ))
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFeature(
            mockApplicationContext,
            MediaSessionServiceDelegate::class.java,
            store
        )

        feature.start()
        verify(mockApplicationContext, never()).startForegroundService(any())

        store.dispatch(MediaSessionAction.ActivatedMediaSessionAction(store.state.tabs[0].id, mock()))

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()
        verify(mockApplicationContext, never()).startForegroundService(any())

        store.dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(store.state.tabs[0].id,
            MediaSession.PlaybackState.PLAYING))
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()
        verify(mockApplicationContext).startForegroundService(any())
    }
}
