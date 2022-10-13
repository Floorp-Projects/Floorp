/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.app.Activity
import android.content.pm.PackageManager
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.verification.VerificationMode
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class PictureInPictureFeatureTest {

    private val crashReporting: CrashReporting = mock()
    private val activity: Activity = Mockito.mock(Activity::class.java, Mockito.RETURNS_DEEP_STUBS)

    @Before
    fun setUp() {
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(true)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `on home pressed without system feature on android m and lower`() {
        val store = mock<BrowserStore>()
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(false)

        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        assertFalse(pictureInPictureFeature.onHomePressed())
        verifyNoInteractions(store)
        verifyNoInteractions(activity.packageManager)
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `on home pressed without system feature on android n and above`() {
        val store = mock<BrowserStore>()
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(false)

        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        assertFalse(pictureInPictureFeature.onHomePressed())
        verifyNoInteractions(store)
        verify(activity.packageManager).hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed without a selected session`() {
        val store = BrowserStore()
        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(activity.packageManager).hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session without a fullscreen mode`() {
        val selectedSession = createTab("https://mozilla.org").copyWithFullScreen(false)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(selectedSession),
                selectedTabId = selectedSession.id,
            ),
        )
        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        assertFalse(selectedSession.content.fullScreen)
        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session in fullscreen without media playing and without pip mode`() {
        val controller = mock<MediaSession.Controller>()
        val selectedSession = createTab(
            url = "https://mozilla.org",
            mediaSessionState = MediaSessionState(
                playbackState = MediaSession.PlaybackState.UNKNOWN,
                controller = controller,
            ),
        ).copyWithFullScreen(true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(selectedSession),
                selectedTabId = selectedSession.id,
            ),
        )
        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        doReturn(false).`when`(pictureInPictureFeature).enterPipModeCompat()

        assertFalse(selectedSession.mediaSessionState?.playbackState == MediaSession.PlaybackState.PLAYING)
        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session in fullscreen with media playing and without pip mode`() {
        val controller = mock<MediaSession.Controller>()
        val selectedSession = createTab(
            url = "https://mozilla.org",
            mediaSessionState = MediaSessionState(
                playbackState = MediaSession.PlaybackState.PLAYING,
                controller = controller,
            ),
        ).copyWithFullScreen(true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(selectedSession),
                selectedTabId = selectedSession.id,
            ),
        )
        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        doReturn(false).`when`(pictureInPictureFeature).enterPipModeCompat()

        assertTrue(selectedSession.mediaSessionState?.playbackState == MediaSession.PlaybackState.PLAYING)
        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(pictureInPictureFeature).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session in fullscreen without media playing and with pip mode`() {
        val controller = mock<MediaSession.Controller>()
        val selectedSession = createTab(
            url = "https://mozilla.org",
            mediaSessionState = MediaSessionState(
                playbackState = MediaSession.PlaybackState.UNKNOWN,
                controller = controller,
            ),
        ).copyWithFullScreen(true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(selectedSession),
                selectedTabId = selectedSession.id,
            ),
        )
        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        doReturn(true).`when`(pictureInPictureFeature).enterPipModeCompat()

        assertFalse(selectedSession.mediaSessionState?.playbackState == MediaSession.PlaybackState.PLAYING)
        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session in fullscreen with media playing and with pip mode`() {
        val controller = mock<MediaSession.Controller>()
        val selectedSession = createTab(
            url = "https://mozilla.org",
            mediaSessionState = MediaSessionState(
                playbackState = MediaSession.PlaybackState.PLAYING,
                controller = controller,
            ),
        ).copyWithFullScreen(true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(selectedSession),
                selectedTabId = selectedSession.id,
            ),
        )
        val pictureInPictureFeature =
            spy(PictureInPictureFeature(store, activity, crashReporting))

        doReturn(true).`when`(pictureInPictureFeature).enterPipModeCompat()

        assertTrue(selectedSession.mediaSessionState?.playbackState == MediaSession.PlaybackState.PLAYING)
        assertTrue(pictureInPictureFeature.onHomePressed())
        verify(pictureInPictureFeature).enterPipModeCompat()
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `enter pip mode compat on android m and below`() {
        val store = mock<BrowserStore>()
        val pictureInPictureFeature = PictureInPictureFeature(store, activity, crashReporting)

        assertFalse(pictureInPictureFeature.enterPipModeCompat())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `enter pip mode compat without system feature on android o`() {
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(false)

        val pictureInPictureFeature =
            PictureInPictureFeature(mock(), activity, crashReporting)

        assertFalse(pictureInPictureFeature.enterPipModeCompat())
        verify(activity, never()).enterPictureInPictureMode(any())
        verifyDeprecatedPictureInPictureMode(activity, never())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `enter pip mode compat with system feature on android o but entering throws exception`() {
        val controller = mock<MediaSession.Controller>()
        val selectedSession = createTab(
            url = "https://mozilla.org",
            mediaSessionState = MediaSessionState(
                playbackState = MediaSession.PlaybackState.PLAYING,
                controller = controller,
            ),
        ).copyWithFullScreen(true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(selectedSession),
                selectedTabId = selectedSession.id,
            ),
        )

        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(true)
        whenever(activity.enterPictureInPictureMode(any())).thenThrow(IllegalStateException())

        val pictureInPictureFeature =
            PictureInPictureFeature(store, activity, crashReporting)
        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(crashReporting).submitCaughtException(any<IllegalStateException>())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `enter pip mode compat on android o and above`() {
        val pictureInPictureFeature =
            PictureInPictureFeature(mock(), activity, crashReporting)

        whenever(activity.enterPictureInPictureMode(any())).thenReturn(true)

        assertTrue(pictureInPictureFeature.enterPipModeCompat())
        verify(activity).enterPictureInPictureMode(any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `enter pip mode compat on android n and above`() {
        val pictureInPictureFeature =
            PictureInPictureFeature(mock(), activity, crashReporting)

        assertTrue(pictureInPictureFeature.enterPipModeCompat())
        verifyDeprecatedPictureInPictureMode(activity)
    }

    @Test
    fun `on pip mode changed`() {
        val store = mock<BrowserStore>()
        val pipFeature = PictureInPictureFeature(
            store,
            activity,
            crashReporting,
            tabId = "tab-id",
        )

        pipFeature.onPictureInPictureModeChanged(true)
        verify(store).dispatch(ContentAction.PictureInPictureChangedAction("tab-id", true))

        pipFeature.onPictureInPictureModeChanged(false)
        verify(store).dispatch(ContentAction.PictureInPictureChangedAction("tab-id", false))

        verify(activity, never()).enterPictureInPictureMode(any())
        verifyDeprecatedPictureInPictureMode(activity, never())
    }

    @Suppress("Deprecation")
    private fun verifyDeprecatedPictureInPictureMode(
        activity: Activity,
        mode: VerificationMode = times(1),
    ) {
        verify(activity, mode).enterPictureInPictureMode()
    }

    @Suppress("Unchecked_Cast")
    private fun <T : SessionState> T.copyWithFullScreen(fullScreen: Boolean): T =
        createCopy(content = content.copy(fullScreen = fullScreen)) as T
}
