/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.fullscreen

import android.app.Activity
import android.content.pm.ActivityInfo
import android.os.Build
import android.view.Window
import android.view.WindowManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.atLeastOnce
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
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = false,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
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
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
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
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
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
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
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
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
        )

        feature.start()
        activity.enterPictureInPictureMode()
        store.waitUntilIdle()
        store.dispatch(ContentAction.PictureInPictureChangedAction("tab1", true))
        store.waitUntilIdle()
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity.requestedOrientation)

        val tab2 = createTab(
            url = "https://firefox.com",
            id = "tab2",
        )
        store.dispatch(TabListAction.AddTabAction(tab2, select = true))
        store.dispatch(
            MediaSessionAction.UpdateMediaFullscreenAction(
                store.state.tabs[0].id,
                false,
                MediaSession.ElementMetadata(),
            ),
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
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
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
                MediaSession.ElementMetadata(),
            ),
        )
        store.waitUntilIdle()

        assertEquals(ActivityInfo.SCREEN_ORIENTATION_USER, activity.requestedOrientation)
    }

    @Suppress("Deprecation")
    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN the currently selected tab is in pip mode WHEN a custom tab loads THEN display custom tab in device's current orientation`() {
        val activity = Robolectric.buildActivity(Activity::class.java).setup().get()
        val elementMetadata = MediaSession.ElementMetadata()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)

        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
        )

        feature.start()
        activity.enterPictureInPictureMode()
        store.waitUntilIdle()

        store.dispatch(ContentAction.PictureInPictureChangedAction("tab1", true))
        store.waitUntilIdle()
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED, activity.requestedOrientation)

        val customTab = createCustomTab(
            "https://www.mozilla.org",
            source = SessionState.Source.Internal.CustomTab,
            id = "tab2",
        )
        store.dispatch(CustomTabListAction.AddCustomTabAction(customTab)).joinBlocking()
        val externalActivity = Robolectric.buildActivity(Activity::class.java).setup().get()
        assertEquals(1, store.state.customTabs.size)
        store.waitUntilIdle()
        val featureForExternalAppBrowser = MediaSessionFullscreenFeature(
            externalActivity,
            store,
            "tab2",
        )
        featureForExternalAppBrowser.start()

        assertNotEquals(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, externalActivity.requestedOrientation)
    }

    @Test
    fun `GIVEN the selected tab in fullscreen mode WHEN the media is paused or stopped THEN release the wake lock of the device`() {
        val activity: Activity = mock()
        val window: Window = mock()

        whenever(activity.window).thenReturn(window)

        val elementMetadata = MediaSession.ElementMetadata()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "tab1",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true,
                    ),
                ),
            ),
            selectedTabId = "tab1",
        )
        val store = BrowserStore(initialState)

        val feature = MediaSessionFullscreenFeature(
            activity,
            store,
            null,
        )
        feature.start()
        verify(activity.window).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        store.dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction("tab1", MediaSession.PlaybackState.PAUSED))
        store.waitUntilIdle()
        verify(activity.window).clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        store.dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction("tab1", MediaSession.PlaybackState.PLAYING))
        store.waitUntilIdle()
        verify(activity.window, atLeastOnce()).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        store.dispatch(MediaSessionAction.DeactivatedMediaSessionAction("tab1"))
        store.waitUntilIdle()
        verify(activity.window, atLeastOnce()).clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }
}
