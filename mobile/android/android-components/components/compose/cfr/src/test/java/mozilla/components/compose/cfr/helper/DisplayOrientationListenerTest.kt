/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.cfr.helper

import android.content.Context
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.content.res.Resources
import android.hardware.display.DisplayManager
import android.os.Build
import android.view.Display
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [Build.VERSION_CODES.N])
class DisplayOrientationListenerTest {
    private val context: Context = mock()
    private val displayManager: DisplayManager = mock()

    @Before
    fun setup() {
        doReturn(displayManager).`when`(context).getSystemService(Context.DISPLAY_SERVICE)

        val display: Display = mock()
        doReturn(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT).`when`(display).rotation
        doReturn(display).`when`(displayManager).getDisplay(0)
    }

    @Test
    fun `WHEN started THEN register it as a display listener`() {
        val listener = DisplayOrientationListener(context) { }

        listener.start()

        verify(displayManager).registerDisplayListener(listener, null)
    }

    @Test
    fun `WHEN stopped THEN unregister from being a display listener`() {
        val listener = DisplayOrientationListener(context) { }

        listener.stop()

        verify(displayManager).unregisterDisplayListener(listener)
    }

    @Test
    fun `WHEN a display is added THEN don't inform the client`() {
        var hasRotationChanged = false
        val listener = DisplayOrientationListener(context) { hasRotationChanged = true }

        listener.onDisplayAdded(1)

        assertFalse(hasRotationChanged)
    }

    @Test
    fun `WHEN a display is removed THEN don't inform the client`() {
        var hasRotationChanged = false
        val listener = DisplayOrientationListener(context) { hasRotationChanged = true }

        listener.onDisplayRemoved(1)

        assertFalse(hasRotationChanged)
    }

    @Test
    fun `GIVEN display is null WHEN a display is changed THEN don't inform the client`() {
        val onDisplayRotationChanged = mock<() -> Unit>()
        val listener = DisplayOrientationListener(context, onDisplayRotationChanged)
        doReturn(null).`when`(displayManager).getDisplay(1)

        listener.onDisplayChanged(1)

        verifyNoInteractions(onDisplayRotationChanged)
    }

    @Test
    fun `WHEN a display is changed but doesn't have a new rotation THEN don't inform the client`() {
        var hasRotationChanged = false
        val listener = DisplayOrientationListener(context) { hasRotationChanged = true }
        val display: Display = mock()
        doReturn(listener.currentOrientation).`when`(display).rotation
        doReturn(display).`when`(displayManager).getDisplay(1)

        listener.onDisplayChanged(1)

        assertFalse(hasRotationChanged)
    }

    @Test
    fun `GIVEN an old Android version WHEN a display is changed and has a new rotation THEN inform the client and remember the new rotation`() {
        var hasRotationChanged = false
        val listener = DisplayOrientationListener(context) { hasRotationChanged = true }
        val display: Display = mock()
        doReturn(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE).`when`(display).rotation
        doReturn(display).`when`(displayManager).getDisplay(1)

        listener.onDisplayChanged(1)

        assertTrue(hasRotationChanged)
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, listener.currentOrientation)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.S])
    fun `GIVEN a new Android version WHEN a display is changed and has a new rotation THEN inform the client and remember the new rotation`() {
        var hasRotationChanged = false
        val config = Configuration().apply {
            orientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        }
        val resources: Resources = mock()
        doReturn(config).`when`(resources).configuration
        doReturn(resources).`when`(context).resources
        val listener = DisplayOrientationListener(context) { hasRotationChanged = true }

        config.orientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
        listener.onDisplayChanged(1)

        assertTrue(hasRotationChanged)
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE, listener.currentOrientation)
    }
}
