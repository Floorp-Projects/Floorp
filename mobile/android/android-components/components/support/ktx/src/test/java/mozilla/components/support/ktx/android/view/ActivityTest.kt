/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.view.View
import android.view.Window
import android.view.WindowManager
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class ActivityTest {

    @Test
    fun `check enterImmersibleMode sets the correct flags`() {
        val activity = mock(Activity::class.java)
        val mockWindow = mock(Window::class.java)
        val mockDecorView = mock(View::class.java)
        val expectedFlags = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_FULLSCREEN
            or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)

        `when`(activity.window).thenReturn(mockWindow)
        `when`(mockWindow.decorView).thenReturn(mockDecorView)

        activity.enterToImmersiveMode()
        verify(mockWindow).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(mockDecorView).systemUiVisibility = expectedFlags
    }

    @Test
    fun `check exitImmersiveModeIfNeeded sets the correct flags`() {
        val activity = mock(Activity::class.java)
        val window = mock(Window::class.java)
        val decorView = mock(View::class.java)
        val attributes = mock(WindowManager.LayoutParams::class.java)
        val expectedFlags = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)

        `when`(activity.window).thenReturn(window)
        `when`(window.decorView).thenReturn(decorView)
        `when`(window.attributes).thenReturn(attributes)
        attributes.flags = 0

        activity.exitImmersiveModeIfNeeded()

        verify(window, never()).clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView, never()).systemUiVisibility = expectedFlags

        attributes.flags = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON

        activity.exitImmersiveModeIfNeeded()

        verify(window).clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility = expectedFlags
    }
}