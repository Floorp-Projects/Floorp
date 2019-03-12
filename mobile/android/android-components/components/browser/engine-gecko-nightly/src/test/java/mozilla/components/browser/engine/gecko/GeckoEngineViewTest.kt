/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import android.graphics.Bitmap
import androidx.test.core.app.ApplicationProvider
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GeckoEngineViewTest {

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun render() {
        val engineView = GeckoEngineView(context)
        val engineSession = mock(GeckoEngineSession::class.java)
        val geckoSession = mock(GeckoSession::class.java)
        val geckoView = mock(NestedGeckoView::class.java)

        `when`(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.currentGeckoView = geckoView

        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)

        `when`(geckoView.session).thenReturn(geckoSession)
        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)
    }

    @Test
    fun captureThumbnail() {
        val engineView = GeckoEngineView(context)
        val mockGeckoView = mock(NestedGeckoView::class.java)
        var thumbnail: Bitmap? = null

        var geckoResult = GeckoResult<Bitmap>()
        `when`(mockGeckoView.capturePixels()).thenReturn(geckoResult)
        engineView.currentGeckoView = mockGeckoView

        engineView.captureThumbnail {
            thumbnail = it
        }

        geckoResult.complete(mock())
        assertNotNull(thumbnail)

        geckoResult = GeckoResult()
        `when`(mockGeckoView.capturePixels()).thenReturn(geckoResult)

        engineView.captureThumbnail {
            thumbnail = it
        }

        geckoResult.completeExceptionally(mock())
        assertNull(thumbnail)
    }
}
