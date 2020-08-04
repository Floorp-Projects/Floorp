/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.fullscreen

import android.app.Activity
import android.content.pm.ActivityInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MediaFullscreenOrientationFeatureTest {
    private val testDispatcher = TestCoroutineDispatcher()
    private lateinit var activity: Activity

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        activity = mock()
        Dispatchers.setMain(testDispatcher)
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `media exiting fullscreen should request SCREEN_ORIENTATION_USER`() {
        val store = BrowserStore(
            BrowserState(
                media = MediaState(
                    MediaState.Aggregate(
                        activeFullscreenOrientation = MediaState.FullscreenOrientation.LANDSCAPE
                    )
                )
            )
        )

        val fullscreenFeature = MediaFullscreenOrientationFeature(activity, store)

        fullscreenFeature.start()

        store.dispatch(
            MediaAction.UpdateMediaAggregateAction(
                MediaState.Aggregate(
                    activeFullscreenOrientation = null
                )
            )
        ).joinBlocking()

        testDispatcher.advanceUntilIdle()
        verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
    }

    @Test
    fun `media entering fullscreen portrait should request portrait orientation`() {
        val store = BrowserStore(
            BrowserState()
        )
        val fullscreenFeature = MediaFullscreenOrientationFeature(activity, store)

        fullscreenFeature.start()

        store.dispatch(
            MediaAction.UpdateMediaAggregateAction(
                MediaState.Aggregate(
                    activeFullscreenOrientation = MediaState.FullscreenOrientation.PORTRAIT
                )
            )
        ).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
    }

    @Test
    fun `media entering fullscreen landscape should request landscape orientation`() {
        val store = BrowserStore(
            BrowserState()
        )
        val fullscreenFeature = MediaFullscreenOrientationFeature(activity, store)

        fullscreenFeature.start()

        store.dispatch(
            MediaAction.UpdateMediaAggregateAction(
                MediaState.Aggregate(
                    activeFullscreenOrientation = MediaState.FullscreenOrientation.LANDSCAPE
                )
            )
        ).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
    }

    @Test
    fun `should ignore initial fullscreen orientation status`() {
        // The orientation is either null or already set. Nothing to do.

        val store = BrowserStore(
            BrowserState()
        )
        val fullscreenFeature = spy(MediaFullscreenOrientationFeature(activity, store))

        fullscreenFeature.start()

        verify(fullscreenFeature, never()).onChanged(any())
    }

    @Test
    fun `should ignore initial fullscreen orientation status and act upon future changes`() {
        // The orientation is either null or already set. Nothing to do.
        // We should act only upon subsequent media orientation changes.

        val store = BrowserStore(
            BrowserState(
                media = MediaState(
                    MediaState.Aggregate(
                        activeFullscreenOrientation = MediaState.FullscreenOrientation.PORTRAIT
                    )
                )
            )
        )
        val fullscreenFeature = spy(MediaFullscreenOrientationFeature(activity, store))

        fullscreenFeature.start()
        store.dispatch(
            MediaAction.UpdateMediaAggregateAction(
                MediaState.Aggregate(
                    activeFullscreenOrientation = MediaState.FullscreenOrientation.LANDSCAPE
                )
            )
        ).joinBlocking()
        testDispatcher.advanceUntilIdle()

        verify(fullscreenFeature, times(1)).onChanged(any())
        verify(fullscreenFeature, times(1)).onChanged(MediaState.FullscreenOrientation.LANDSCAPE)
    }
}
