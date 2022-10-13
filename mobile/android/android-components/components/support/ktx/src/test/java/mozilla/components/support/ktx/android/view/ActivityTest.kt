/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.view.View
import android.view.ViewTreeObserver
import android.view.Window
import android.view.WindowInsets
import androidx.core.view.WindowInsetsCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@Suppress("DEPRECATION")
@RunWith(AndroidJUnit4::class)
class ActivityTest {

    private lateinit var activity: Activity
    private lateinit var window: Window
    private lateinit var decorView: View
    private lateinit var viewTreeObserver: ViewTreeObserver
    private lateinit var windowInsetsCompat: WindowInsetsCompat
    private lateinit var windowInsets: WindowInsets

    @Before
    fun setup() {
        activity = mock()
        window = mock()
        decorView = mock()
        viewTreeObserver = mock()
        windowInsetsCompat = mock()
        windowInsets = mock()

        `when`(activity.window).thenReturn(window)
        `when`(window.decorView).thenReturn(decorView)
        `when`(window.decorView.viewTreeObserver).thenReturn(viewTreeObserver)
        `when`(windowInsetsCompat.toWindowInsets()).thenReturn(windowInsets)
        `when`(window.decorView.onApplyWindowInsets(windowInsets)).thenReturn(windowInsets)
    }

    @Test
    fun `check enterToImmersiveMode sets the correct flags`() {
        activity.enterToImmersiveMode()

        // verify entering immersive mode
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        // verify that the immersive mode restoration is set as expected
        verify(window.decorView).setOnApplyWindowInsetsListener(any())
    }

    @Test
    fun `check setAsImmersive sets the correct flags`() {
        activity.setAsImmersive()

        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        verify(window.decorView, never()).setOnSystemUiVisibilityChangeListener(any())
    }

    @Test
    fun `check enableImmersiveModeRestore sets insets listeners`() {
        activity.enableImmersiveModeRestore()

        verify(window.decorView).setOnApplyWindowInsetsListener(any())
    }

    @Test
    fun `check enableImmersiveModeRestore set insets listeners have the correct behavior when status bar is visible`() {
        val insetListenerCaptor = argumentCaptor<View.OnApplyWindowInsetsListener>()

        doReturn(30).`when`(windowInsets).systemWindowInsetTop

        activity.enableImmersiveModeRestore()
        verify(window.decorView).setOnApplyWindowInsetsListener(insetListenerCaptor.capture())

        insetListenerCaptor.value.onApplyWindowInsets(window.decorView, windowInsets)

        // Cannot test if "setAsImmersive()" was called it being an extension function but we can check the effect of that call.
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
    }

    @Test
    fun `check enableImmersiveModeRestore set insets listeners have the correct behavior when status bar is NOT visible`() {
        val insetListenerCaptor = argumentCaptor<View.OnApplyWindowInsetsListener>()

        doReturn(0).`when`(windowInsets).systemWindowInsetTop

        activity.enableImmersiveModeRestore()
        verify(window.decorView).setOnApplyWindowInsetsListener(insetListenerCaptor.capture())

        insetListenerCaptor.value.onApplyWindowInsets(window.decorView, windowInsets)

        // Cannot test if "setAsImmersive()" was called it being an extension function but we can check the effect of that call.
        verify(window, never()).addFlags(anyInt())
        verify(decorView, never()).systemUiVisibility = anyInt()
        verify(decorView, never()).systemUiVisibility = anyInt()
    }

    @Test
    fun `check exitImmersiveMode sets the correct flags`() {
        activity.exitImmersiveMode()

        verify(decorView, times(2)).systemUiVisibility = View.SYSTEM_UI_FLAG_VISIBLE
        verify(decorView).setOnApplyWindowInsetsListener(null)
    }

    @Test
    fun `check exitImmersiveMode correctly cleanups the insets listeners`() {
        activity.exitImmersiveMode()

        verify(decorView).setOnApplyWindowInsetsListener(null)
    }
}
