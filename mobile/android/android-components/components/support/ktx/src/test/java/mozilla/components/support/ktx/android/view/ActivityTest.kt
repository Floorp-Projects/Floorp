/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.os.Build
import android.view.View
import android.view.ViewTreeObserver
import android.view.Window
import android.view.WindowInsets
import android.view.WindowManager
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.annotation.Config

@Suppress("DEPRECATION")
@RunWith(AndroidJUnit4::class)
class ActivityTest {

    private lateinit var activity: Activity
    private lateinit var window: Window
    private lateinit var decorView: View
    private lateinit var viewTreeObserver: ViewTreeObserver
    private lateinit var windowInsets: WindowInsets
    private lateinit var insetsController: WindowInsetsControllerCompat
    private lateinit var layoutParams: WindowManager.LayoutParams

    @Before
    fun setup() {
        activity = mock()
        window = mock()
        decorView = mock()
        viewTreeObserver = mock()
        windowInsets = mock()
        insetsController = mock()
        layoutParams = WindowManager.LayoutParams()

        `when`(activity.window).thenReturn(window)
        `when`(window.decorView).thenReturn(decorView)
        `when`(window.decorView.viewTreeObserver).thenReturn(viewTreeObserver)
        `when`(window.decorView.onApplyWindowInsets(windowInsets)).thenReturn(windowInsets)
        `when`(window.attributes).thenReturn(layoutParams)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P])
    fun `GIVEN Android version P WHEN enterImmersiveMode is called THEN systems bars are hidden, inset listener is set and notch flags are set to extend view into notch area`() {
        activity.enterImmersiveMode(insetsController)

        verify(insetsController).hide(WindowInsetsCompat.Type.systemBars())
        verify(insetsController).systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        verify(window.decorView).setOnApplyWindowInsetsListener(any())

        verify(window).setFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS, WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)
        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES, layoutParams.layoutInDisplayCutoutMode)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O_MR1])
    fun `GIVEN Android version O_MR1 WHEN enterImmersiveMode is called THEN systems bars are hidden, inset listener is set and notch flags are not being set`() {
        activity.enterImmersiveMode(insetsController)

        verify(insetsController).hide(WindowInsetsCompat.Type.systemBars())
        verify(insetsController).systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        verify(window.decorView).setOnApplyWindowInsetsListener(any())

        verify(window, never()).setFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS, WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)
    }

    @Test
    @Config(sdk = [28])
    fun `GIVEN enterImmersiveMode was called WHEN window insets are changed THEN insetsController hides system bars and sets bars behaviour again`() {
        val insetListenerCaptor = argumentCaptor<View.OnApplyWindowInsetsListener>()
        doReturn(30).`when`(windowInsets).systemWindowInsetTop

        activity.enterImmersiveMode(insetsController)

        verify(insetsController, times(1)).hide(WindowInsetsCompat.Type.systemBars())
        verify(insetsController, times(1)).systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        verify(window.decorView).setOnApplyWindowInsetsListener(insetListenerCaptor.capture())
        insetListenerCaptor.value.onApplyWindowInsets(window.decorView, windowInsets)

        verify(insetsController, times(2)).hide(WindowInsetsCompat.Type.systemBars())
        verify(insetsController, times(2)).systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    @Test
    fun `GIVEN enterImmersiveMode was called WHEN window insets are not changed THEN insetsController does nothing`() {
        val insetListenerCaptor = argumentCaptor<View.OnApplyWindowInsetsListener>()
        doReturn(0).`when`(windowInsets).systemWindowInsetTop

        activity.enterImmersiveMode(insetsController)

        verify(insetsController, times(1)).hide(WindowInsetsCompat.Type.systemBars())
        verify(insetsController, times(1)).systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        verify(window.decorView).setOnApplyWindowInsetsListener(insetListenerCaptor.capture())
        insetListenerCaptor.value.onApplyWindowInsets(window.decorView, windowInsets)

        verify(insetsController, times(1)).hide(WindowInsetsCompat.Type.systemBars())
        verify(insetsController, times(1)).systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    @Test
    fun `WHEN exitImmersiveMode is called THEN insetsController shows system bars and OnApplyWindowInsetsListener is cleared`() {
        activity.exitImmersiveMode(insetsController)

        verify(insetsController).show(WindowInsetsCompat.Type.systemBars())
        verify(window.decorView).setOnApplyWindowInsetsListener(null)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P])
    fun `GIVEN Android version P WHEN exitImmersiveMode is called THEN notch flags are reset to defaults`() {
        activity.exitImmersiveMode()

        verify(window).clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)
        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT, layoutParams.layoutInDisplayCutoutMode)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O_MR1])
    fun `GIVEN Android version O_MR1 WHEN exitImmersiveMode is called THEN notch flags were not being set`() {
        activity.exitImmersiveMode()

        verify(window, never()).setFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS, WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)
    }
}
