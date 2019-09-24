/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.app.Activity
import android.content.pm.PackageManager
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
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
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.verifyZeroInteractions
import org.mockito.verification.VerificationMode
import org.robolectric.annotation.Config

private interface PipChangedCallback : (Boolean) -> Unit

@RunWith(AndroidJUnit4::class)
class PictureInPictureFeatureTest {

    private val sessionManager: SessionManager = mock()
    private val selectedSession: Session = mock()
    private val activity: Activity = Mockito.mock(Activity::class.java, Mockito.RETURNS_DEEP_STUBS)

    @Before
    fun setUp() {
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(true)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `on home pressed without system feature on android m and lower`() {
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(false)

        val pictureInPictureFeature = spy(PictureInPictureFeature(sessionManager, activity))

        assertFalse(pictureInPictureFeature.onHomePressed())
        verifyZeroInteractions(sessionManager)
        verifyZeroInteractions(activity.packageManager)
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `on home pressed without system feature on android n and above`() {
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(false)

        val pictureInPictureFeature = spy(PictureInPictureFeature(sessionManager, activity))

        assertFalse(pictureInPictureFeature.onHomePressed())
        verifyZeroInteractions(sessionManager)
        verify(activity.packageManager).hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed without a selected session`() {
        val pictureInPictureFeature = spy(PictureInPictureFeature(sessionManager, activity))

        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(sessionManager).selectedSession
        verify(activity.packageManager).hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session without a fullscreen mode`() {
        val pictureInPictureFeature = spy(PictureInPictureFeature(sessionManager, activity))

        whenever(selectedSession.fullScreenMode).thenReturn(false)
        whenever(sessionManager.selectedSession).thenReturn(selectedSession)

        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(sessionManager).selectedSession
        verify(selectedSession).fullScreenMode
        verify(pictureInPictureFeature, never()).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session in fullscreen and without pip mode`() {
        val pictureInPictureFeature = spy(PictureInPictureFeature(sessionManager, activity))

        whenever(selectedSession.fullScreenMode).thenReturn(true)
        whenever(sessionManager.selectedSession).thenReturn(selectedSession)
        doReturn(false).`when`(pictureInPictureFeature).enterPipModeCompat()

        assertFalse(pictureInPictureFeature.onHomePressed())
        verify(sessionManager).selectedSession
        verify(selectedSession).fullScreenMode
        verify(pictureInPictureFeature).enterPipModeCompat()
    }

    @Test
    fun `on home pressed with a selected session in fullscreen and with pip mode`() {
        val pictureInPictureFeature = spy(PictureInPictureFeature(sessionManager, activity))

        whenever(selectedSession.fullScreenMode).thenReturn(true)
        whenever(sessionManager.selectedSession).thenReturn(selectedSession)
        doReturn(true).`when`(pictureInPictureFeature).enterPipModeCompat()

        assertTrue(pictureInPictureFeature.onHomePressed())
        verify(sessionManager).selectedSession
        verify(selectedSession).fullScreenMode
        verify(pictureInPictureFeature).enterPipModeCompat()
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `enter pip mode compat on android m and below`() {
        val pictureInPictureFeature = PictureInPictureFeature(sessionManager, activity)

        assertFalse(pictureInPictureFeature.enterPipModeCompat())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `enter pip mode compat without system feature on android o`() {
        whenever(activity.packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
            .thenReturn(false)

        val pictureInPictureFeature = PictureInPictureFeature(sessionManager, activity)

        assertFalse(pictureInPictureFeature.enterPipModeCompat())
        verify(activity, never()).enterPictureInPictureMode(any())
        verifyDeprecatedPictureInPictureMode(activity, never())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `enter pip mode compat on android o and above`() {
        val pictureInPictureFeature = PictureInPictureFeature(sessionManager, activity)

        whenever(activity.enterPictureInPictureMode(any())).thenReturn(true)

        assertTrue(pictureInPictureFeature.enterPipModeCompat())
        verify(activity).enterPictureInPictureMode(any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `enter pip mode compat on android n and above`() {
        val pictureInPictureFeature = PictureInPictureFeature(sessionManager, activity)

        assertTrue(pictureInPictureFeature.enterPipModeCompat())
        verifyDeprecatedPictureInPictureMode(activity)
    }

    @Test
    fun `on pip mode changed`() {
        val pipChangedCallback: PipChangedCallback = mock()
        val pipFeature = PictureInPictureFeature(sessionManager, activity, pipChangedCallback)

        pipFeature.onPictureInPictureModeChanged(true)
        verify(pipChangedCallback).invoke(true)

        pipFeature.onPictureInPictureModeChanged(false)
        verify(pipChangedCallback).invoke(false)

        verifyNoMoreInteractions(sessionManager)
        verify(activity, never()).enterPictureInPictureMode(any())
        verifyDeprecatedPictureInPictureMode(activity, never())
    }
}

@Suppress("DEPRECATION")
private fun verifyDeprecatedPictureInPictureMode(activity: Activity, mode: VerificationMode = times(1)) {
    verify(activity, mode).enterPictureInPictureMode()
}
