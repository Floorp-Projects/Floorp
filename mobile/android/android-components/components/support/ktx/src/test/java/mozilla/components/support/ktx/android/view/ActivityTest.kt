/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.view.View
import android.view.ViewTreeObserver
import android.view.Window
import android.view.WindowManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@Suppress("DEPRECATION")
@RunWith(AndroidJUnit4::class)
class ActivityTest {

    private lateinit var activity: Activity
    private lateinit var window: Window
    private lateinit var decorView: View
    private lateinit var viewTreeObserver: ViewTreeObserver

    @Before
    fun setup() {
        activity = mock()
        window = mock()
        decorView = mock()
        viewTreeObserver = mock()

        `when`(activity.window).thenReturn(window)
        `when`(window.decorView).thenReturn(decorView)
        `when`(window.decorView.viewTreeObserver).thenReturn(viewTreeObserver)
    }

    @Test
    fun `check enterToImmersiveMode sets the correct flags`() {

        activity.enterToImmersiveMode()

        // verify entering immersive mode
        verify(window).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        // verify that the immersive mode restoration is set as expected
        verify(window.decorView.viewTreeObserver, never()).addOnWindowFocusChangeListener(any())
        verify(window.decorView).setOnSystemUiVisibilityChangeListener(any())
    }

    @Test
    fun `check setAsImmersive sets the correct flags`() {
        activity.setAsImmersive()

        verify(window).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        verify(window.decorView.viewTreeObserver, never()).addOnWindowFocusChangeListener(any())
    }

    @Test
    fun `check enableImmersiveModeRestore sets the correct insets listeners`() {
        activity.enableImmersiveModeRestore(null)
        verify(window.decorView.viewTreeObserver, never()).addOnWindowFocusChangeListener(any())
        verify(window.decorView).setOnSystemUiVisibilityChangeListener(any())

        val windowFocusChangeListener: ViewTreeObserver.OnWindowFocusChangeListener = mock()
        activity.enableImmersiveModeRestore(windowFocusChangeListener)
        verify(window.decorView.viewTreeObserver).addOnWindowFocusChangeListener(windowFocusChangeListener)
        verify(window.decorView, times(2)).setOnSystemUiVisibilityChangeListener(any())
    }

    @Test
    fun `check enableImmersiveModeRestore set insets listeners have the correct behavior`() {
        val insetListenerCaptor = argumentCaptor<View.OnSystemUiVisibilityChangeListener>()
        activity.enableImmersiveModeRestore(null)
        verify(window.decorView).setOnSystemUiVisibilityChangeListener(insetListenerCaptor.capture())

        insetListenerCaptor.value.onSystemUiVisibilityChange(View.SYSTEM_UI_FLAG_FULLSCREEN)
        // If the activity requested to enter fullscreen no immersive mode restoration is needed.
        verify(window, never()).addFlags(anyInt())
        verify(decorView, never()).systemUiVisibility = anyInt()
        verify(decorView, never()).systemUiVisibility = anyInt()

        insetListenerCaptor.value.onSystemUiVisibilityChange(View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR)
        // Cannot test if "setAsImmersive()" was called it being an extension function but we can check the effect of that call.
        verify(window).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
    }

    @Test
    fun `check exitImmersiveModeIfNeeded sets the correct flags`() {
        val attributes = mock(WindowManager.LayoutParams::class.java)
        `when`(window.attributes).thenReturn(attributes)
        attributes.flags = 0

        activity.exitImmersiveModeIfNeeded()

        verify(window, never()).clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView, never()).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN.inv()
        verify(decorView, never()).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION.inv()
        verify(decorView, never()).setOnSystemUiVisibilityChangeListener(null)

        attributes.flags = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON

        activity.exitImmersiveModeIfNeeded()

        verify(window).clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView, times(2)).systemUiVisibility = View.SYSTEM_UI_FLAG_VISIBLE
        verify(window.decorView.viewTreeObserver, never()).removeOnWindowFocusChangeListener(any())
        verify(decorView).setOnSystemUiVisibilityChangeListener(null)
    }

    @Test
    fun `check exitImmersiveModeIfNeeded with callback removes the correct callback`() {
        val callback: ViewTreeObserver.OnWindowFocusChangeListener = mock()
        val attributes = mock(WindowManager.LayoutParams::class.java)
        attributes.flags = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        `when`(window.attributes).thenReturn(attributes)

        activity.exitImmersiveModeIfNeeded(callback)

        verify(window.decorView.viewTreeObserver).removeOnWindowFocusChangeListener(callback)
    }
}
