/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.isVisible
import mozilla.components.browser.engine.gecko.GeckoEngineView
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.widgets.behavior.EngineViewClippingBehavior
import mozilla.components.ui.widgets.behavior.EngineViewScrollingBehavior
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.focus.R
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
internal class BrowserToolbarTest {
    private val parent = CoordinatorLayout(testContext)
    private val toolbar = spy(BrowserToolbar(testContext))
    private val engineView = spy(GeckoEngineView(testContext))
    private val toolbarHeight = testContext.resources.getDimensionPixelSize(R.dimen.browser_toolbar_height)

    init {
        doReturn(toolbarHeight).`when`(toolbar).height
        parent.addView(toolbar)
        parent.addView(engineView)
    }

    @Test
    fun `GIVEN a BrowserToolbar WHEN enableDynamicBehavior THEN set custom behaviors for the toolbar and engineView`() {
        // Simulate previously having a fixed toolbar
        (engineView.layoutParams as? CoordinatorLayout.LayoutParams)?.topMargin = 222

        toolbar.enableDynamicBehavior(testContext, engineView)

        assertTrue((toolbar.layoutParams as? CoordinatorLayout.LayoutParams)?.behavior is EngineViewScrollingBehavior)
        assertTrue((engineView.layoutParams as? CoordinatorLayout.LayoutParams)?.behavior is EngineViewClippingBehavior)
        assertEquals(0, (engineView.layoutParams as? CoordinatorLayout.LayoutParams)?.topMargin)
        verify(engineView).setDynamicToolbarMaxHeight(toolbarHeight)
    }

    @Test
    fun `GIVEN a BrowserToolbar WHEN disableDynamicBehavior THEN set null behaviors for the toolbar and engineView`() {
        engineView.asView().translationY = 123f

        toolbar.disableDynamicBehavior(engineView)

        assertNull((toolbar.layoutParams as? CoordinatorLayout.LayoutParams)?.behavior)
        assertNull((engineView.layoutParams as? CoordinatorLayout.LayoutParams)?.behavior)
        assertEquals(0f, engineView.asView().translationY)
        verify(engineView).setDynamicToolbarMaxHeight(0)
    }

    @Test
    fun `GIVEN a BrowserToolbar WHEN showAsFixed is called THEN show the toolbar with the engineView below it`() {
        toolbar.showAsFixed(testContext, engineView)

        verify(toolbar).isVisible = true
        verify(engineView).setDynamicToolbarMaxHeight(0)
        assertEquals(toolbarHeight, (engineView.layoutParams as? CoordinatorLayout.LayoutParams)?.topMargin)
    }

    @Test
    fun `GIVEN a BrowserToolbar WHEN hide is called THEN show the toolbar with the engineView below it`() {
        toolbar.hide(engineView)

        verify(toolbar).isVisible = false
        verify(engineView).setDynamicToolbarMaxHeight(0)
        assertEquals(0, (engineView.layoutParams as? CoordinatorLayout.LayoutParams)?.topMargin)
    }
}
