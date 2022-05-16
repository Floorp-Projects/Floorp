/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.fullscreen

import android.app.Activity
import android.content.pm.ActivityInfo
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class MediaSessionFullscreenFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `GIVEN the currently selected tab is not in fullscreen WHEN the feature is running THEN orientation is set to default`() {
        val activity: Activity = mock()
        val elementMetadata = MediaSession.ElementMetadata()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org", id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = false
                    )
                )
            ),
            selectedTabId = "tab1"
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store
        )

        feature.start()

        verify(activity).setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_USER)
    }

    @Test
    fun `GIVEN the currently selected tab plays portrait media WHEN the feature is running THEN orientation is set to portrait`() {
        val activity: Activity = mock()
        val elementMetadata = MediaSession.ElementMetadata(width = 360, height = 640)
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org", id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true
                    )
                )
            ),
            selectedTabId = "tab1"
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store
        )

        feature.start()

        verify(activity).setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT)
    }

    @Test
    fun `GIVEN the currently selected tab plays landscape media WHEN it enters fullscreen THEN set orientation to landscape`() {
        val activity: Activity = mock()
        val elementMetadata = MediaSession.ElementMetadata(width = 640, height = 360)
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org", id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true
                    )
                )
            ),
            selectedTabId = "tab1"
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store
        )

        feature.start()

        verify(activity).setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE)
    }

    @Suppress("Deprecation")
    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN the currently selected tab plays landscape media WHEN it enters pip mode THEN set orientation to unspecified`() {
        val activity = Robolectric.buildActivity(Activity::class.java).setup().get()
        val elementMetadata = MediaSession.ElementMetadata()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org", id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true
                    )
                )
            ),
            selectedTabId = "tab1"
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store
        )

        feature.start()
        activity.enterPictureInPictureMode()
        store.waitUntilIdle()

        assertTrue(activity.isInPictureInPictureMode)
        store.dispatch(ContentAction.PictureInPictureChangedAction("tab1", true))
        store.waitUntilIdle()

        assertEquals(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity.requestedOrientation)
    }

    @Suppress("Deprecation")
    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN the currently selected tab is in pip mode WHEN an external intent arrives THEN set orientation to default`() {
        val activity = Robolectric.buildActivity(Activity::class.java).setup().get()
        val elementMetadata = MediaSession.ElementMetadata()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org", id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true
                    )
                )
            ),
            selectedTabId = "tab1"
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store
        )

        feature.start()
        activity.enterPictureInPictureMode()
        store.waitUntilIdle()
        store.dispatch(ContentAction.PictureInPictureChangedAction("tab1", true))
        store.waitUntilIdle()
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity.requestedOrientation)

        val tab2 = createTab(
            url = "https://firefox.com", id = "tab2"
        )
        store.dispatch(TabListAction.AddTabAction(tab2, select = true))
        store.dispatch(
            MediaSessionAction.UpdateMediaFullscreenAction(
                store.state.tabs[0].id,
                false,
                MediaSession.ElementMetadata()
            )
        )
        store.waitUntilIdle()
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_USER, activity.requestedOrientation)
        assertEquals(tab2.id, store.state.selectedTabId)
    }

    @Suppress("Deprecation")
    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN the currently selected tab is in pip mode WHEN it exits pip mode THEN set orientation to default`() {
        val activity = Robolectric.buildActivity(Activity::class.java).setup().get()
        val elementMetadata = MediaSession.ElementMetadata()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org", id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true
                    )
                )
            ),
            selectedTabId = "tab1"
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store
        )

        feature.start()
        activity.enterPictureInPictureMode()
        store.waitUntilIdle()

        assertTrue(activity.isInPictureInPictureMode)
        store.dispatch(ContentAction.PictureInPictureChangedAction("tab1", true))
        store.waitUntilIdle()

        assertEquals(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity.requestedOrientation)

        store.dispatch(
            MediaSessionAction.UpdateMediaFullscreenAction(
                store.state.tabs[0].id,
                false,
                MediaSession.ElementMetadata()
            )
        )
        store.waitUntilIdle()

        assertEquals(ActivityInfo.SCREEN_ORIENTATION_USER, activity.requestedOrientation)
    }
}
