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
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@Suppress("Deprecation")
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
    fun `check enterImmersibleMode sets the correct flags`() {

        activity.enterToImmersiveMode()

        verify(window).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN
        verify(decorView).systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        verify(window.decorView.viewTreeObserver, never()).addOnWindowFocusChangeListener(any())
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

        attributes.flags = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON

        activity.exitImmersiveModeIfNeeded()

        verify(window).clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView, times(2)).systemUiVisibility = View.SYSTEM_UI_FLAG_VISIBLE
        verify(window.decorView.viewTreeObserver, never()).removeOnWindowFocusChangeListener(any())
    }

    @Test
    fun `check enterImmersibleMode with callback adds the correct callback`() {
        val callback: ViewTreeObserver.OnWindowFocusChangeListener = mock()

        activity.enterToImmersiveMode(callback)

        verify(window.decorView.viewTreeObserver).addOnWindowFocusChangeListener(callback)
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
