/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.behavior

import android.content.Context
import android.view.View
import android.widget.EditText
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
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

@RunWith(AndroidJUnit4::class)
class EngineViewBrowserToolbarBehaviorTest {

    @Test
    fun `EngineView clipping and bottom toolbar offset are kept in sync`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val toolbar: View = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(42f).`when`(toolbar).translationY

        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView.asView(),
            toolbar.height,
            ToolbarPosition.BOTTOM,
        )

        behavior.onDependentViewChanged(mock(), mock(), toolbar)

        verify(engineView).setVerticalClipping(-42)
        assertEquals(0f, engineView.asView().translationY)
    }

    @Test
    fun `EngineView clipping and top toolbar offset are kept in sync`() {
        val engineView: EngineView = spy(FakeEngineView(testContext))
        val toolbar: View = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(42f).`when`(toolbar).translationY

        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView.asView(),
            toolbar.height,
            ToolbarPosition.TOP,
        )

        behavior.onDependentViewChanged(mock(), mock(), toolbar)

        verify(engineView).setVerticalClipping(42)
        assertEquals(142f, engineView.asView().translationY)
    }

    @Test
    fun `Behavior does not depend on normal views`() {
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            mock(),
            0,
            ToolbarPosition.BOTTOM,
        )

        assertFalse(behavior.layoutDependsOn(mock(), mock(), TextView(testContext)))
        assertFalse(behavior.layoutDependsOn(mock(), mock(), EditText(testContext)))
        assertFalse(behavior.layoutDependsOn(mock(), mock(), ImageView(testContext)))
    }

    @Test
    fun `Behavior depends on BrowserToolbar`() {
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            mock(),
            0,
            ToolbarPosition.BOTTOM,
        )

        assertTrue(behavior.layoutDependsOn(mock(), mock(), BrowserToolbar(testContext)))
    }

    @Test
    fun `GIVEN a bottom toolbar WHEN translation has below a half decimal THEN set vertical clipping with the floor value`() {
        val engineView: FakeEngineView = mock()
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView,
            100,
            ToolbarPosition.BOTTOM,
        )

        behavior.toolbarChangedAction(40.4f)

        verify(engineView).setVerticalClipping(-40)
    }

    @Test
    fun `GIVEN a bottom toolbar WHEN translation has exactly half of a decimal THEN set vertical clipping with the ceiling value`() {
        val engineView: FakeEngineView = mock()
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView,
            100,
            ToolbarPosition.BOTTOM,
        )

        behavior.toolbarChangedAction(40.5f)

        verify(engineView).setVerticalClipping(-41)
    }

    @Test
    fun `GIVEN a bottom toolbar WHEN translation has more than a half decimal THEN set vertical clipping with the ceiling value`() {
        val engineView: FakeEngineView = mock()
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView,
            100,
            ToolbarPosition.BOTTOM,
        )

        behavior.toolbarChangedAction(40.6f)

        verify(engineView).setVerticalClipping(-41)
    }

    @Test
    fun `GIVEN a top toolbar WHEN translation has below a half decimal THEN set vertical clipping with the floor value`() {
        val engineView: FakeEngineView = mock()
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView,
            100,
            ToolbarPosition.TOP,
        )

        behavior.toolbarChangedAction(40.4f)

        verify(engineView).setVerticalClipping(40)
    }

    @Test
    fun `GIVEN a top toolbar WHEN translation has exactly half of a decimal THEN set vertical clipping with the ceiling value`() {
        val engineView: FakeEngineView = mock()
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView,
            100,
            ToolbarPosition.TOP,
        )

        behavior.toolbarChangedAction(40.5f)

        verify(engineView).setVerticalClipping(41)
    }

    @Test
    fun `GIVEN a top toolbar WHEN translation has more than a half decimal THEN set vertical clipping with the ceiling value`() {
        val engineView: FakeEngineView = mock()
        val behavior = EngineViewBrowserToolbarBehavior(
            mock(),
            null,
            engineView,
            100,
            ToolbarPosition.TOP,
        )

        behavior.toolbarChangedAction(40.6f)

        verify(engineView).setVerticalClipping(41)
    }
}

class BrowserToolbar(context: Context) : TextView(context)
