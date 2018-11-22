/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.window

import android.os.Message
import android.webkit.WebView
import mozilla.components.browser.engine.system.SystemEngineSession
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class SystemWindowRequestTest {

    @Test
    fun `init request`() {
        val curWebView = mock(WebView::class.java)
        val newWebView = mock(WebView::class.java)
        val request = SystemWindowRequest(curWebView, newWebView, true, true)

        assertTrue(request.openAsDialog)
        assertTrue(request.triggeredByUser)
        assertEquals("about:blank", request.url)
    }

    @Test
    fun `prepare sets webview on engine session`() {
        val curWebView = mock(WebView::class.java)
        val newWebView = mock(WebView::class.java)
        val request = SystemWindowRequest(curWebView, newWebView)

        val engineSession = SystemEngineSession()
        request.prepare(engineSession)
        assertSame(newWebView, engineSession.webView)
    }

    @Test
    fun `start sends message to target`() {
        val curWebView = mock(WebView::class.java)
        val newWebView = mock(WebView::class.java)
        val resultMsg = mock(Message::class.java)

        SystemWindowRequest(curWebView, newWebView, false, false).start()
        verify(resultMsg, never()).sendToTarget()

        SystemWindowRequest(curWebView, newWebView, false, false, resultMsg).start()
        verify(resultMsg, never()).sendToTarget()

        resultMsg.obj = ""
        SystemWindowRequest(curWebView, newWebView, false, false, resultMsg).start()
        verify(resultMsg, never()).sendToTarget()

        resultMsg.obj = mock(WebView.WebViewTransport::class.java)
        SystemWindowRequest(curWebView, newWebView, false, false, resultMsg).start()
        verify(resultMsg, times(1)).sendToTarget()
    }
}