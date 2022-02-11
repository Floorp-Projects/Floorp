/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.compose

import android.content.Context
import android.graphics.PixelFormat
import android.view.View
import android.view.ViewManager
import android.view.WindowManager
import androidx.lifecycle.ViewTreeLifecycleOwner
import androidx.savedstate.ViewTreeSavedStateRegistryOwner
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CFRPopupTest {
    @Test
    fun `WHEN the popup is constructed THEN setup lifecycle owners`() {
        val container = View(testContext)

        val popupView = spy(CFRPopupFullScreenLayout(container, mock(), "", mock(), mock()))

        assertEquals(ViewTreeLifecycleOwner.get(container), ViewTreeLifecycleOwner.get(popupView))
        assertEquals(ViewTreeSavedStateRegistryOwner.get(container), ViewTreeSavedStateRegistryOwner.get(popupView))
    }

    @Test
    fun `WHEN the popup is dismissed THEN cleanup lifecycle owners and detach from window`() {
        val context = spy(testContext)
        val container = View(context)
        val windowManager = spy(context.getSystemService(Context.WINDOW_SERVICE)) as WindowManager
        doReturn(windowManager).`when`(context).getSystemService(Context.WINDOW_SERVICE)
        val popupView = spy(CFRPopupFullScreenLayout(container, mock(), "", mock(), mock()))
        popupView.show()

        popupView.dismiss()

        assertEquals(null, ViewTreeLifecycleOwner.get(popupView))
        assertEquals(null, ViewTreeSavedStateRegistryOwner.get(popupView))
        verify(popupView).disposeComposition()
        verify(windowManager).removeViewImmediate(popupView)
    }

    @Test
    fun `GIVEN a popup WHEN adding it to window THEN use translucent layout params`() {
        val context = spy(testContext)
        val container = View(context)
        val windowManager = spy(context.getSystemService(Context.WINDOW_SERVICE))
        doReturn(windowManager).`when`(context).getSystemService(Context.WINDOW_SERVICE)
        val popupView = CFRPopupFullScreenLayout(
            container, mock(), "", mock(), mock()
        )
        // The equality check in `verify` fails for the layout params.
        // Seems like we have to verify `addView`s arguments manually.
        val layoutParamsCaptor = argumentCaptor<WindowManager.LayoutParams>()

        popupView.show()

        verify(windowManager as ViewManager).addView(eq(popupView), layoutParamsCaptor.capture())
        assertEquals(WindowManager.LayoutParams.TYPE_APPLICATION_PANEL, layoutParamsCaptor.value.type)
        assertEquals(container.applicationWindowToken, layoutParamsCaptor.value.token)
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, layoutParamsCaptor.value.width)
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, layoutParamsCaptor.value.height)
        assertEquals(PixelFormat.TRANSLUCENT, layoutParamsCaptor.value.format)
        assertEquals(
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
            layoutParamsCaptor.value.flags
        )
    }

    @Test
    fun `WHEN creating layout params THEN get fullscreen translucent layout params`() {
        val container = View(testContext)
        val popupView = CFRPopupFullScreenLayout(
            container, mock(), "", mock(), mock()
        )

        val result = popupView.createLayoutParams()

        assertEquals(WindowManager.LayoutParams.TYPE_APPLICATION_PANEL, result.type)
        assertEquals(container.applicationWindowToken, result.token)
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, result.width)
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, result.height)
        assertEquals(PixelFormat.TRANSLUCENT, result.format)
        assertEquals(
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
            result.flags
        )
    }
}
