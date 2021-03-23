/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.app.Activity
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineView.Companion.DARK_COVER
import mozilla.components.browser.engine.gecko.selection.GeckoSelectionActionDelegate
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MockSelection
import org.robolectric.Robolectric.buildActivity

@RunWith(AndroidJUnit4::class)
class GeckoEngineViewTest {

    private val context: Context
        get() = buildActivity(Activity::class.java).get()

    @Test
    fun render() {
        val engineView = GeckoEngineView(context)
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.geckoView = geckoView

        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)

        whenever(geckoView.session).thenReturn(geckoSession)
        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)
    }

    @Test
    fun captureThumbnail() {
        val engineView = GeckoEngineView(context)
        val mockGeckoView = mock<NestedGeckoView>()
        var thumbnail: Bitmap? = null

        var geckoResult = GeckoResult<Bitmap>()
        whenever(mockGeckoView.capturePixels()).thenReturn(geckoResult)
        engineView.geckoView = mockGeckoView

        // Test GeckoResult resolves successfuly
        engineView.captureThumbnail {
            thumbnail = it
        }
        verify(mockGeckoView).capturePixels()
        geckoResult.complete(mock())
        assertNotNull(thumbnail)

        geckoResult = GeckoResult()
        whenever(mockGeckoView.capturePixels()).thenReturn(geckoResult)

        // Test GeckoResult resolves in error
        engineView.captureThumbnail {
            thumbnail = it
        }
        geckoResult.completeExceptionally(mock())
        assertNull(thumbnail)

        // Test GeckoView throwing an exception
        whenever(mockGeckoView.capturePixels()).thenThrow(IllegalStateException("Compositor not ready"))

        thumbnail = mock()
        engineView.captureThumbnail {
            thumbnail = it
        }
        assertNull(thumbnail)
    }

    @Test
    fun `clearSelection is forwarded to BasicSelectionAction instance`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()
        engineView.currentSelection = mock()

        engineView.clearSelection()

        verify(engineView.currentSelection)?.clearSelection()
    }

    @Test
    fun `setColorScheme uses preferred color scheme to set correct cover color`() {
        val engineView = GeckoEngineView(context)

        engineView.geckoView = mock()

        var preferredColorScheme: PreferredColorScheme = PreferredColorScheme.Light

        engineView.setColorScheme(preferredColorScheme)

        verify(engineView.geckoView)?.coverUntilFirstPaint(Color.WHITE)

        preferredColorScheme = PreferredColorScheme.Dark
        engineView.setColorScheme(preferredColorScheme)
        verify(engineView.geckoView)?.coverUntilFirstPaint(DARK_COVER)
    }

    @Test
    fun `setVerticalClipping is forwarded to GeckoView instance`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()

        engineView.setVerticalClipping(-42)

        verify(engineView.geckoView).setVerticalClipping(-42)
    }

    @Test
    fun `setDynamicToolbarMaxHeight is forwarded to GeckoView instance`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()

        engineView.setDynamicToolbarMaxHeight(42)

        verify(engineView.geckoView).setDynamicToolbarMaxHeight(42)
    }

    @Test
    fun `release method releases session from GeckoView`() {
        val engineView = GeckoEngineView(context)
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.geckoView = geckoView

        engineView.render(engineSession)

        verify(geckoView, never()).releaseSession()

        engineView.release()

        verify(geckoView).releaseSession()
    }

    @Test
    fun `after rendering currentSelection should be a GeckoSelectionActionDelegate`() {
        val engineView = GeckoEngineView(context).apply {
            selectionActionDelegate = mock()
        }
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.geckoView = geckoView

        engineView.render(engineSession)

        assertTrue(engineView.currentSelection is GeckoSelectionActionDelegate)
    }

    @Test
    fun `will attach and detach selection action delegate when rendering and releasing`() {
        val delegate: SelectionActionDelegate = mock()

        val engineView = GeckoEngineView(context).apply {
            selectionActionDelegate = delegate
        }
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.geckoView = geckoView

        engineView.render(engineSession)

        val captor = argumentCaptor<GeckoSession.SelectionActionDelegate>()
        verify(geckoSession).selectionActionDelegate = captor.capture()

        assertTrue(captor.value is GeckoSelectionActionDelegate)
        val capturedDelegate = captor.value as GeckoSelectionActionDelegate

        assertEquals(delegate, capturedDelegate.customDelegate)

        verify(geckoSession, never()).selectionActionDelegate = null

        engineView.release()

        verify(geckoSession).selectionActionDelegate = null
    }

    @Test
    fun `will attach and detach selection action delegate when rendering new session`() {
        val delegate: SelectionActionDelegate = mock()

        val engineView = GeckoEngineView(context).apply {
            selectionActionDelegate = delegate
        }
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.geckoView = geckoView

        engineView.render(engineSession)

        val captor = argumentCaptor<GeckoSession.SelectionActionDelegate>()
        verify(geckoSession).selectionActionDelegate = captor.capture()

        assertTrue(captor.value is GeckoSelectionActionDelegate)
        val capturedDelegate = captor.value as GeckoSelectionActionDelegate

        assertEquals(delegate, capturedDelegate.customDelegate)

        verify(geckoSession, never()).selectionActionDelegate = null

        whenever(geckoView.session).thenReturn(geckoSession)

        engineView.render(mock<GeckoEngineSession>().apply {
            whenever(this.geckoSession).thenReturn(mock())
        })

        verify(geckoSession).selectionActionDelegate = null
    }

    @Test
    fun `setVisibility is propagated to gecko view`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()

        engineView.visibility = View.GONE
        verify(engineView.geckoView)?.visibility = View.GONE
    }

    @Test
    fun `canClearSelection should return false for null selection, null and empty selection text`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()
        engineView.currentSelection = mock()

        // null selection returns false
        whenever(engineView.currentSelection?.selection).thenReturn(null)
        assertFalse(engineView.canClearSelection())

        // selection with null text returns false
        val selectionWthNullText: GeckoSession.SelectionActionDelegate.Selection = mock()
        whenever(engineView.currentSelection?.selection).thenReturn(selectionWthNullText)
        assertFalse(engineView.canClearSelection())

        // selection with empty text returns false
        val bundle = GeckoBundle()
        bundle.putString("selection", "")
        val selectionWthEmptyText: GeckoSession.SelectionActionDelegate.Selection = MockSelection(bundle)
        whenever(engineView.currentSelection?.selection).thenReturn(selectionWthEmptyText)
        assertFalse(engineView.canClearSelection())
    }

    @Suppress("Deprecation")
    @Test
    fun `currentInputResult should default to EngineView#InputResult#INPUT_RESULT_UNHANDLED`() {
        val engineView = GeckoEngineView(context)

        assertEquals(EngineView.InputResult.INPUT_RESULT_UNHANDLED, engineView.lastInputResult)
    }

    @Suppress("Deprecation")
    @Test
    fun `getInputResult should do a 1-1 mapping of the values received from GeckoView and cache the result`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()

        whenever(engineView.geckoView.inputResult).thenReturn(0)
        assertEquals(EngineView.InputResult.INPUT_RESULT_UNHANDLED, engineView.getInputResult())
        assertEquals(EngineView.InputResult.INPUT_RESULT_UNHANDLED, engineView.lastInputResult)

        whenever(engineView.geckoView.inputResult).thenReturn(1)
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED, engineView.getInputResult())
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED, engineView.lastInputResult)

        whenever(engineView.geckoView.inputResult).thenReturn(2)
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED_CONTENT, engineView.getInputResult())
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED_CONTENT, engineView.lastInputResult)
    }

    @Suppress("Deprecation")
    @Test
    fun `INPUT_RESULD_IGNORED should be ignored`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()

        whenever(engineView.geckoView.inputResult).thenReturn(1)
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED, engineView.getInputResult())
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED, engineView.lastInputResult)

        whenever(engineView.geckoView.inputResult).thenReturn(3)
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED, engineView.getInputResult())
        assertEquals(EngineView.InputResult.INPUT_RESULT_HANDLED, engineView.lastInputResult)
    }

    @Suppress("Deprecation")
    @Test(expected = IllegalArgumentException::class)
    fun `Values other than 0, 1, 2, 3 received as input results from GeckoView should throw`() {
        val engineView = GeckoEngineView(context)
        engineView.geckoView = mock()

        whenever(engineView.geckoView.inputResult).thenReturn(4)
        engineView.getInputResult()
    }
}
