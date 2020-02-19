/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.app.Activity
import android.content.Context
import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.selection.GeckoSelectionActionDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
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
        engineView.currentGeckoView = geckoView

        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)

        whenever(geckoView.session).thenReturn(geckoSession)
        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)
    }

    @Test
    fun captureThumbnail() {
        val geckoSession: GeckoEngineSession = mock()
        val engineView = GeckoEngineView(context)
        val mockGeckoView = mock<NestedGeckoView>()
        var thumbnail: Bitmap? = null

        var geckoResult = GeckoResult<Bitmap>()
        whenever(mockGeckoView.capturePixels()).thenReturn(geckoResult)
        engineView.currentGeckoView = mockGeckoView

        engineView.captureThumbnail {
            thumbnail = it
        }
        verify(mockGeckoView, never()).capturePixels()

        engineView.currentSession = geckoSession
        engineView.captureThumbnail {
            thumbnail = it
        }
        verify(mockGeckoView, never()).capturePixels()

        whenever(geckoSession.firstContentfulPaint).thenReturn(true)
        engineView.captureThumbnail {
            thumbnail = it
        }
        verify(mockGeckoView).capturePixels()
        geckoResult.complete(mock())
        assertNotNull(thumbnail)

        geckoResult = GeckoResult()
        whenever(mockGeckoView.capturePixels()).thenReturn(geckoResult)

        engineView.captureThumbnail {
            thumbnail = it
        }

        geckoResult.completeExceptionally(mock())
        assertNull(thumbnail)

        // Verify that with `firstContentfulPaint` set to false, capturePixels returns a null bitmap
        geckoResult = GeckoResult()

        thumbnail = mock()
        whenever(geckoSession.firstContentfulPaint).thenReturn(false)
        engineView.captureThumbnail {
            thumbnail = it
        }
        // capturePixels should not have been called again because `firstContentfulPaint` is false
        verify(mockGeckoView, times(2)).capturePixels()
        geckoResult.complete(mock())
        assertNull(thumbnail)

        // Verify that with `firstContentfulPaint` set to false, capturePixels returns a null bitmap
        geckoResult = GeckoResult()

        thumbnail = mock()
        whenever(geckoSession.firstContentfulPaint).thenReturn(false)
        engineView.captureThumbnail {
            thumbnail = it
        }
        // capturePixels should not have been called again because `firstContentfulPaint` is false
        verify(mockGeckoView, times(2)).capturePixels()
        geckoResult.complete(mock())
        assertNull(thumbnail)
    }

    @Test
    fun `clearSelection is forwarded to BasicSelectionAction instance`() {
        val engineView = GeckoEngineView(context)
        engineView.currentGeckoView = mock()
        engineView.currentSelection = mock()

        engineView.clearSelection()

        verify(engineView.currentSelection)?.clearSelection()
    }

    @Test
    fun `setVerticalClipping is forwarded to GeckoView instance`() {
        val engineView = GeckoEngineView(context)
        engineView.currentGeckoView = mock()

        engineView.setVerticalClipping(-42)

        verify(engineView.currentGeckoView).setVerticalClipping(-42)
    }

    @Test
    fun `setDynamicToolbarMaxHeight is forwarded to GeckoView instance`() {
        val engineView = GeckoEngineView(context)
        engineView.currentGeckoView = mock()

        engineView.setDynamicToolbarMaxHeight(42)

        verify(engineView.currentGeckoView).setDynamicToolbarMaxHeight(42)
    }

    @Test
    fun `release method releases session from GeckoView`() {
        val engineView = GeckoEngineView(context)
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.currentGeckoView = geckoView

        engineView.render(engineSession)

        verify(geckoView, never()).releaseSession()
        verify(engineSession, never()).unregister(any())

        engineView.release()

        verify(geckoView).releaseSession()
        verify(engineSession).unregister(any())
    }

    @Test
    fun `View will rebind session if process gets killed`() {
        val engineView = GeckoEngineView(context)
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.currentGeckoView = geckoView

        engineView.render(engineSession)

        reset(geckoView)
        verify(geckoView, never()).setSession(geckoSession)

        engineView.observer.onProcessKilled()

        verify(geckoView).setSession(geckoSession)
    }

    @Test
    fun `View will rebind session if session crashed`() {
        val engineView = GeckoEngineView(context)
        val engineSession = mock<GeckoEngineSession>()
        val geckoSession = mock<GeckoSession>()
        val geckoView = mock<NestedGeckoView>()

        whenever(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.currentGeckoView = geckoView

        engineView.render(engineSession)

        reset(geckoView)
        verify(geckoView, never()).setSession(geckoSession)

        engineView.observer.onCrash()

        verify(geckoView).setSession(geckoSession)
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
        engineView.currentGeckoView = geckoView

        engineView.render(engineSession)

        assertTrue(engineView.currentSelection is GeckoSelectionActionDelegate)
    }
}
