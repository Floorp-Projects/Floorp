/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.cfr

import android.content.Context
import android.graphics.PixelFormat
import android.view.View
import android.view.ViewManager
import android.view.WindowManager
import android.view.WindowManager.LayoutParams
import androidx.lifecycle.ViewTreeLifecycleOwner
import androidx.lifecycle.findViewTreeLifecycleOwner
import androidx.savedstate.findViewTreeSavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CFRPopupFullscreenLayoutTest {
    @Test
    fun `WHEN the popup is constructed THEN setup lifecycle owners`() {
        val anchor = View(testContext).apply {
            ViewTreeLifecycleOwner.set(this, mock())
            this.setViewTreeSavedStateRegistryOwner(mock())
        }

        val popupView = spy(
            CFRPopupFullscreenLayout(
                anchor = anchor,
                properties = mock(),
                onDismiss = mock(),
                text = { },
                action = { },
            ),
        )

        assertNotNull(popupView.findViewTreeLifecycleOwner())
        assertEquals(
            anchor.findViewTreeLifecycleOwner(),
            popupView.findViewTreeLifecycleOwner(),
        )
        assertNotNull(popupView.findViewTreeSavedStateRegistryOwner())
        assertEquals(
            assertNotNull(anchor.findViewTreeSavedStateRegistryOwner()),
            assertNotNull(popupView.findViewTreeSavedStateRegistryOwner()),
        )
    }

    @Test
    fun `WHEN the popup is dismissed THEN cleanup lifecycle owners and detach from window`() {
        val context = spy(testContext)
        val anchor = View(context).apply {
            ViewTreeLifecycleOwner.set(this, mock())
            this.setViewTreeSavedStateRegistryOwner(mock())
        }
        val windowManager = spy(context.getSystemService(Context.WINDOW_SERVICE) as WindowManager)
        doReturn(windowManager).`when`(context).getSystemService(Context.WINDOW_SERVICE)
        val popupView = CFRPopupFullscreenLayout(anchor, mock(), mock(), { }, { })
        popupView.show()
        assertNotNull(popupView.findViewTreeLifecycleOwner())
        assertNotNull(popupView.findViewTreeSavedStateRegistryOwner())

        popupView.dismiss()

        assertNull(popupView.findViewTreeLifecycleOwner())
        assertNull(popupView.findViewTreeSavedStateRegistryOwner())
        verify(windowManager).removeViewImmediate(popupView)
    }

    @Test
    fun `GIVEN a popup WHEN adding it to window THEN use translucent layout params`() {
        val context = spy(testContext)
        val anchor = View(context)
        val windowManager = spy(context.getSystemService(Context.WINDOW_SERVICE))
        doReturn(windowManager).`when`(context).getSystemService(Context.WINDOW_SERVICE)
        val popupView = CFRPopupFullscreenLayout(anchor, mock(), mock(), { }, { })
        val layoutParamsCaptor = argumentCaptor<LayoutParams>()

        popupView.show()

        verify(windowManager as ViewManager).addView(eq(popupView), layoutParamsCaptor.capture())
        assertEquals(LayoutParams.TYPE_APPLICATION_PANEL, layoutParamsCaptor.value.type)
        assertEquals(anchor.applicationWindowToken, layoutParamsCaptor.value.token)
        assertEquals(LayoutParams.MATCH_PARENT, layoutParamsCaptor.value.width)
        assertEquals(LayoutParams.MATCH_PARENT, layoutParamsCaptor.value.height)
        assertEquals(PixelFormat.TRANSLUCENT, layoutParamsCaptor.value.format)
        assertEquals(
            LayoutParams.FLAG_LAYOUT_IN_SCREEN or LayoutParams.FLAG_HARDWARE_ACCELERATED,
            layoutParamsCaptor.value.flags,
        )
    }

    @Test
    fun `WHEN creating layout params THEN get fullscreen translucent layout params`() {
        val anchor = View(testContext)
        val popupView = CFRPopupFullscreenLayout(anchor, mock(), mock(), { }, { })

        val result = popupView.createLayoutParams()

        assertEquals(LayoutParams.TYPE_APPLICATION_PANEL, result.type)
        assertEquals(anchor.applicationWindowToken, result.token)
        assertEquals(LayoutParams.MATCH_PARENT, result.width)
        assertEquals(LayoutParams.MATCH_PARENT, result.height)
        assertEquals(PixelFormat.TRANSLUCENT, result.format)
        assertEquals(
            LayoutParams.FLAG_LAYOUT_IN_SCREEN or LayoutParams.FLAG_HARDWARE_ACCELERATED,
            result.flags,
        )
    }
}
