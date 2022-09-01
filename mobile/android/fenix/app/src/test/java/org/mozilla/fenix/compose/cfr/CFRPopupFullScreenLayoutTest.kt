/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.cfr

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
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class CFRPopupFullScreenLayoutTest {
    @Test
    fun `WHEN the popup is constructed THEN setup lifecycle owners`() {
        val anchor = View(testContext).apply {
            ViewTreeLifecycleOwner.set(this, mockk())
            this.setViewTreeSavedStateRegistryOwner(mockk())
        }

        val popupView = spyk(CFRPopupFullScreenLayout("", anchor, mockk(), mockk()) {})

        assertNotNull(popupView.findViewTreeLifecycleOwner())
        assertEquals(
            anchor.findViewTreeLifecycleOwner(),
            popupView.findViewTreeLifecycleOwner()
        )
        assertNotNull(popupView.findViewTreeSavedStateRegistryOwner())
        assertEquals(
            assertNotNull(anchor.findViewTreeSavedStateRegistryOwner()),
            assertNotNull(popupView.findViewTreeSavedStateRegistryOwner())
        )
    }

    @Test
    fun `WHEN the popup is dismissed THEN cleanup lifecycle owners and detach from window`() {
        val context = spyk(testContext)
        val anchor = View(context).apply {
            ViewTreeLifecycleOwner.set(this, mockk())
            this.setViewTreeSavedStateRegistryOwner(mockk())
        }
        val windowManager = spyk(context.getSystemService(Context.WINDOW_SERVICE) as WindowManager)
        every { context.getSystemService(Context.WINDOW_SERVICE) } returns windowManager
        val popupView = CFRPopupFullScreenLayout("", anchor, mockk(), mockk()) {}
        popupView.show()
        assertNotNull(popupView.findViewTreeLifecycleOwner())
        assertNotNull(popupView.findViewTreeSavedStateRegistryOwner())

        popupView.dismiss()

        assertNull(popupView.findViewTreeLifecycleOwner())
        assertNull(popupView.findViewTreeSavedStateRegistryOwner())
        verify { windowManager.removeViewImmediate(popupView) }
    }

    @Test
    fun `GIVEN a popup WHEN adding it to window THEN use translucent layout params`() {
        val context = spyk(testContext)
        val anchor = View(context)
        val windowManager = spyk(context.getSystemService(Context.WINDOW_SERVICE))
        every { context.getSystemService(Context.WINDOW_SERVICE) } returns windowManager
        val popupView = CFRPopupFullScreenLayout("", anchor, mockk(), mockk()) {}
        val layoutParamsCaptor = slot<LayoutParams>()

        popupView.show()

        verify { (windowManager as ViewManager).addView(eq(popupView), capture(layoutParamsCaptor)) }
        assertEquals(LayoutParams.TYPE_APPLICATION_PANEL, layoutParamsCaptor.captured.type)
        assertEquals(anchor.applicationWindowToken, layoutParamsCaptor.captured.token)
        assertEquals(LayoutParams.MATCH_PARENT, layoutParamsCaptor.captured.width)
        assertEquals(LayoutParams.MATCH_PARENT, layoutParamsCaptor.captured.height)
        assertEquals(PixelFormat.TRANSLUCENT, layoutParamsCaptor.captured.format)
        assertEquals(
            LayoutParams.FLAG_LAYOUT_IN_SCREEN or LayoutParams.FLAG_HARDWARE_ACCELERATED,
            layoutParamsCaptor.captured.flags
        )
    }

    @Test
    fun `WHEN creating layout params THEN get fullscreen translucent layout params`() {
        val anchor = View(testContext)
        val popupView = CFRPopupFullScreenLayout("", anchor, mockk(), mockk()) {}

        val result = popupView.createLayoutParams()

        assertEquals(LayoutParams.TYPE_APPLICATION_PANEL, result.type)
        assertEquals(anchor.applicationWindowToken, result.token)
        assertEquals(LayoutParams.MATCH_PARENT, result.width)
        assertEquals(LayoutParams.MATCH_PARENT, result.height)
        assertEquals(PixelFormat.TRANSLUCENT, result.format)
        assertEquals(
            LayoutParams.FLAG_LAYOUT_IN_SCREEN or LayoutParams.FLAG_HARDWARE_ACCELERATED,
            result.flags
        )
    }
}
