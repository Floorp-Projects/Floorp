/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.fullscreen

import android.app.Activity
import android.content.pm.ActivityInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MediaSessionFullscreenFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `screen orientation is updated correctly`() {
        val mockActivity: Activity = mock()
        val elementMetadata = MediaSession.ElementMetadata()
        val initialState = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    mediaSessionState = MediaSessionState(
                        mock(),
                        elementMetadata = elementMetadata,
                        playbackState = MediaSession.PlaybackState.PLAYING,
                        fullscreen = true
                    )
                )
            )
        )
        val store = BrowserStore(initialState)
        val feature = MediaSessionFullscreenFeature(
            mockActivity,
            store
        )

        feature.start()
        verify(mockActivity).setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE)

        store.dispatch(
            MediaSessionAction.UpdateMediaFullscreenAction(
                store.state.tabs[0].id,
                true,
                MediaSession.ElementMetadata(height = 1L)
            )
        )

        store.waitUntilIdle()

        verify(mockActivity).setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT)
    }
}
