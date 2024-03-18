/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.View
import android.widget.EditText
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.fakes.engine.FakeEngineView
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.toolbar.navbar.EngineViewClippingBehavior
import org.mozilla.fenix.components.toolbar.navbar.ToolbarContainerView

@RunWith(AndroidJUnit4::class)
class EngineViewClippingBehaviorTest {

    // Bottom toolbar position tests
    @Test
    fun `GIVEN the toolbar is at the bottom WHEN toolbar is being shifted THEN EngineView adjusts bottom clipping && EngineViewParent position doesn't change`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbar: BrowserToolbar = mock()
        doReturn(Y_DOWN_TRANSITION).`when`(toolbar).translationY

        assertEquals(0f, engineParentView.translationY)

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = 0,
        ).apply {
            this.engineView = engineView
        }.run {
            onDependentViewChanged(mock(), mock(), toolbar)
        }

        // We want to position the engine view popup content
        // right above the bottom toolbar when the toolbar
        // is being shifted down. The top of the bottom toolbar
        // is either positive or zero, but for clipping
        // the values should be negative because the baseline
        // for clipping is bottom toolbar height.
        val bottomClipping = -Y_DOWN_TRANSITION.toInt()
        verify(engineView).setVerticalClipping(bottomClipping)

        assertEquals(0f, engineParentView.translationY)
    }

    @Test
    fun `GIVEN the toolbar is at the bottom && the navbar is enabled WHEN toolbar is being shifted THEN EngineView adjusts bottom clipping && EngineViewParent position doesn't change`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbar: ToolbarContainerView = mock()
        doReturn(Y_DOWN_TRANSITION).`when`(toolbar).translationY

        assertEquals(0f, engineParentView.translationY)

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = 0,
        ).apply {
            this.engineView = engineView
        }.run {
            onDependentViewChanged(mock(), mock(), toolbar)
        }

        // We want to position the engine view popup content
        // right above the bottom toolbar when the toolbar
        // is being shifted down. The top of the bottom toolbar
        // is either positive or zero, but for clipping
        // the values should be negative because the baseline
        // for clipping is bottom toolbar height.
        val bottomClipping = -Y_DOWN_TRANSITION.toInt()
        verify(engineView).setVerticalClipping(bottomClipping)

        assertEquals(0f, engineParentView.translationY)
    }

    // Top toolbar position tests
    @Test
    fun `GIVEN the toolbar is at the top WHEN toolbar is being shifted THEN EngineView adjusts bottom clipping && EngineViewParent shifts as well`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbar: BrowserToolbar = mock()
        doReturn(Y_UP_TRANSITION).`when`(toolbar).translationY

        assertEquals(0f, engineParentView.translationY)

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = TOOLBAR_HEIGHT.toInt(),
        ).apply {
            this.engineView = engineView
        }.run {
            onDependentViewChanged(mock(), mock(), toolbar)
        }

        verify(engineView).setVerticalClipping(Y_UP_TRANSITION.toInt())

        // Here we are adjusting the vertical position of
        // the engine view container to be directly under
        // the toolbar. The top toolbar is shifting up, so
        // its translation will be either negative or zero.
        val bottomClipping = Y_UP_TRANSITION + TOOLBAR_HEIGHT
        assertEquals(bottomClipping, engineParentView.translationY)
    }

    // Combined toolbar position tests
    @Test
    fun `WHEN both of the toolbars are being shifted GIVEN the toolbar is at the top && the navbar is enabled THEN EngineView adjusts bottom clipping`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbar: BrowserToolbar = mock()
        val toolbarContainerView: ToolbarContainerView = mock()
        doReturn(Y_UP_TRANSITION).`when`(toolbar).translationY
        doReturn(Y_DOWN_TRANSITION).`when`(toolbarContainerView).translationY

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = TOOLBAR_HEIGHT.toInt(),
        ).apply {
            this.engineView = engineView
        }.run {
            onDependentViewChanged(mock(), mock(), toolbar)
            onDependentViewChanged(mock(), mock(), toolbarContainerView)
        }

        val doubleClipping = Y_UP_TRANSITION - Y_DOWN_TRANSITION
        verify(engineView).setVerticalClipping(doubleClipping.toInt())
    }

    @Test
    fun `WHEN both of the toolbars are being shifted GIVEN the toolbar is at the top && the navbar is enabled THEN EngineViewParent shifts as well`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbar: BrowserToolbar = mock()
        val toolbarContainerView: ToolbarContainerView = mock()
        doReturn(Y_UP_TRANSITION).`when`(toolbar).translationY
        doReturn(Y_DOWN_TRANSITION).`when`(toolbarContainerView).translationY

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = TOOLBAR_HEIGHT.toInt(),
        ).apply {
            this.engineView = engineView
        }.run {
            onDependentViewChanged(mock(), mock(), toolbar)
            onDependentViewChanged(mock(), mock(), toolbarContainerView)
        }

        // The top of the parent should be positioned right below the toolbar,
        // so when we are given the new Y position of the top of the toolbar,
        // which is always negative as the element is being "scrolled" out of
        // the screen, the bottom of the toolbar is just a toolbar height away
        // from it.
        val parentTranslation = Y_UP_TRANSITION + TOOLBAR_HEIGHT
        assertEquals(parentTranslation, engineParentView.translationY)
    }

    // Edge cases
    @Test
    fun `GIVEN top toolbar is much bigger than bottom WHEN bottom stopped shifting && top is shifting THEN bottom clipping && engineParentView shifting is still accurate`() {
        val largeYUpTransition = -500f
        val largeTopToolbarHeight = 500
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbar: BrowserToolbar = mock()
        doReturn(largeYUpTransition).`when`(toolbar).translationY

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = largeTopToolbarHeight,
        ).apply {
            this.engineView = engineView
            this.recentBottomToolbarTranslation = Y_DOWN_TRANSITION
        }.run {
            onDependentViewChanged(mock(), mock(), toolbar)
        }

        val doubleClipping = largeYUpTransition - Y_DOWN_TRANSITION
        verify(engineView).setVerticalClipping(doubleClipping.toInt())

        val parentTranslation = largeYUpTransition + largeTopToolbarHeight
        assertEquals(parentTranslation, engineParentView.translationY)
    }

    @Test
    fun `GIVEN bottom toolbar is much bigger than top WHEN top stopped shifting && bottom is shifting THEN bottom clipping && engineParentView shifting is still accurate`() {
        val largeYBottomTransition = 500f
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbarContainerView: ToolbarContainerView = mock()
        doReturn(largeYBottomTransition).`when`(toolbarContainerView).translationY

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = TOOLBAR_HEIGHT.toInt(),
        ).apply {
            this.engineView = engineView
            this.recentTopToolbarTranslation = Y_UP_TRANSITION
        }.run {
            onDependentViewChanged(mock(), mock(), toolbarContainerView)
        }

        val doubleClipping = Y_UP_TRANSITION - largeYBottomTransition
        verify(engineView).setVerticalClipping(doubleClipping.toInt())

        val parentTranslation = Y_UP_TRANSITION + TOOLBAR_HEIGHT
        assertEquals(parentTranslation, engineParentView.translationY)
    }

    @Test
    fun `GIVEN a bottom toolbar WHEN translation returns NaN THEN no exception thrown`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val engineParentView: View = spy(View(testContext))
        val toolbar: View = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(Float.NaN).`when`(toolbar).translationY

        EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = engineParentView,
            topToolbarHeight = 0,
        ).apply {
            this.engineView = engineView
        }.run {
            onDependentViewChanged(mock(), mock(), toolbar)
        }

        assertEquals(0f, engineView.asView().translationY)
    }

    // General tests
    @Test
    fun `WHEN layoutDependsOn receives a class that isn't a ScrollableToolbar THEN it ignores it`() {
        val behavior = EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = mock(),
            topToolbarHeight = 0,
        )

        assertFalse(behavior.layoutDependsOn(mock(), mock(), TextView(testContext)))
        assertFalse(behavior.layoutDependsOn(mock(), mock(), EditText(testContext)))
        assertFalse(behavior.layoutDependsOn(mock(), mock(), ImageView(testContext)))
    }

    @Test
    fun `WHEN layoutDependsOn receives a class that is a ScrollableToolbar THEN it recognizes it as a dependency`() {
        val behavior = EngineViewClippingBehavior(
            context = mock(),
            attrs = null,
            engineViewParent = mock(),
            topToolbarHeight = 0,
        )

        assertTrue(behavior.layoutDependsOn(mock(), mock(), BrowserToolbar(testContext)))
        assertTrue(behavior.layoutDependsOn(mock(), mock(), ToolbarContainerView(testContext)))
    }
}

private const val TOOLBAR_HEIGHT = 100f
private const val Y_UP_TRANSITION = -42f
private const val Y_DOWN_TRANSITION = -42f
