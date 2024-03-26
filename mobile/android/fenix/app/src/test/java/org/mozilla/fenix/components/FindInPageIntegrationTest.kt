/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.engine.gecko.GeckoEngineView
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test

class FindInPageIntegrationTest {
    // For ease of tests naming "find in page bar" is referred to as FIPB.

    @Test
    fun `GIVEN FIPB not shown WHEN prepareLayoutForFindBar is called for a dynamic top toolbar THEN toolbar is hidden and browser translated up`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = true,
            isToolbarPlacedAtTop = true,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 123)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.prepareLayoutForFindBar()

        verify { toolbar.isVisible = false }
        verify { engineViewParent.translationY = 0f }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So I used a real instance and assert on the value actually set.
        assertEquals(123, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FIPB not shown WHEN prepareLayoutForFindBar is called for a fixed top toolbar THEN toolbar is hidden and browser translated up`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = false,
            isToolbarPlacedAtTop = true,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 100)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.prepareLayoutForFindBar()

        verify { toolbar.isVisible = false }
        verify { engineViewParent.translationY = -123f }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So a real instance is used to then assert on the value actually set.
        assertEquals(0, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FIPB shown WHEN restorePreviousLayout is called for a dynamic top toolbar THEN toolbar is shown and browser translated down`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = true,
            isToolbarPlacedAtTop = true,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 50)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.restorePreviousLayout()

        verify { toolbar.isVisible = true }
        verify { engineViewParent.translationY = 123f }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So I used a real instance and assert on the value actually set.
        assertEquals(0, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FIPB shown WHEN restorePreviousLayout is called for a fixed top toolbar THEN toolbar is shown and browser translated down`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = false,
            isToolbarPlacedAtTop = true,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 50)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.restorePreviousLayout()

        verify { toolbar.isVisible = true }
        verify { engineViewParent.translationY = 0f }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So I used a real instance and assert on the value actually set.
        assertEquals(0, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FIPB not shown WHEN prepareLayoutForFindBar is called for a dynamic bottom toolbar THEN toolbar is hidden and browser is made smaller`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = true,
            isToolbarPlacedAtTop = false,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 50)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.prepareLayoutForFindBar()

        verify { toolbar.isVisible = false }
        verify(exactly = 0) { engineViewParent.translationY = any() }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So I used a real instance and assert on the value actually set.
        assertEquals(123, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FIPB not shown WHEN prepareLayoutForFindBar is called for a fixed bottom toolbar THEN toolbar is hidden and browser remains the same`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = false,
            isToolbarPlacedAtTop = false,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 50)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.prepareLayoutForFindBar()

        verify { toolbar.isVisible = false }
        verify(exactly = 0) { engineViewParent.translationY = any() }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So I used a real instance and assert on the value actually set.
        assertEquals(123, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FIPB shown WHEN restorePreviousLayout is called for a dynamic bottom toolbar THEN toolbar is shown and browser is made bigger`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = true,
            isToolbarPlacedAtTop = false,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 50)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.restorePreviousLayout()

        verify { toolbar.isVisible = true }
        verify(exactly = 0) { engineViewParent.translationY = any() }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So I used a real instance and assert on the value actually set.
        assertEquals(0, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FIPB shown WHEN restorePreviousLayout is called for a fixed bottom toolbar THEN toolbar is shown and browser remains the same`() {
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 123
        }
        val engineViewParent: FrameLayout = mockk(relaxed = true)
        val engineViewParentParams = ViewGroup.MarginLayoutParams(100, 100)
        val toolbarInfo = FindInPageIntegration.ToolbarInfo(
            toolbar = toolbar,
            isToolbarDynamic = true,
            isToolbarPlacedAtTop = false,
        )
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), mockk(), toolbarInfo, 50)) {
            every { getEngineViewsParentLayoutParams() } returns engineViewParentParams
            every { getEngineViewParent() } returns engineViewParent
        }

        feature.restorePreviousLayout()

        verify { toolbar.isVisible = true }
        verify(exactly = 0) { engineViewParent.translationY = any() }
        // MockKException: Missing calls inside verify { ... } block if verifying the bottomMargin setter on a mockk
        // So I used a real instance and assert on the value actually set.
        assertEquals(0, engineViewParentParams.bottomMargin)
    }

    @Test
    fun `GIVEN FindInPageIntegration WHEN getEngineViewParent is called THEN it returns EngineView's layout parent`() {
        val parent: FrameLayout = mockk()
        val engineView: GeckoEngineView = mockk(relaxed = true)
        every { (engineView as EngineView).asView().parent } returns parent

        val feature = FindInPageIntegration(mockk(), null, mockk(), engineView, mockk(), 50)

        assertSame(parent as View, feature.getEngineViewParent())
    }

    @Test
    fun `GIVEN FindInPageIntegration WHEN getEngineViewsParentLayoutParams is called THEN it returns EngineView's layout parent MarginLayoutParams`() {
        val parent: FrameLayout = mockk(relaxed = true) {
            every { layoutParams } returns mockk<ViewGroup.MarginLayoutParams>(relaxed = true)
        }
        val engineView: GeckoEngineView = mockk(relaxed = true)
        val feature = spyk(FindInPageIntegration(mockk(), null, mockk(), engineView, mockk(), 60))
        every { feature.getEngineViewParent() } returns parent

        assertSame(parent.layoutParams, feature.getEngineViewsParentLayoutParams())
    }
}
