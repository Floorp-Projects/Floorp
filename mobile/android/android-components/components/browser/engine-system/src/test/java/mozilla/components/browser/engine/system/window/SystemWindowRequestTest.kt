/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.window

import android.os.Message
import android.webkit.WebSettings
import android.webkit.WebView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.system.SystemEngineSession
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SystemWindowRequestTest {

    @Test
    fun `init request`() {
        val curWebView = mock<WebView>()
        val newWebView = mock<WebView>()
        val newEngineSession = mock<SystemEngineSession>()
        val request = SystemWindowRequest(curWebView, newEngineSession, newWebView, true, true)

        assertTrue(request.openAsDialog)
        assertTrue(request.triggeredByUser)
        assertEquals("", request.url)
    }

    @Test
    fun `prepare sets webview on engine session`() {
        val curWebView = mock<WebView>()
        val newWebView = mock<WebView>()
        val settings = mock<WebSettings>()

        whenever(curWebView.settings).thenReturn(settings)
        whenever(newWebView.settings).thenReturn(settings)

        val newEngineSession = SystemEngineSession(testContext)
        val request = SystemWindowRequest(curWebView, newEngineSession, newWebView)

        val engineSession = request.prepare() as SystemEngineSession
        assertSame(newWebView, engineSession.webView)
    }

    @Test
    fun `start sends message to target`() {
        val curWebView = mock<WebView>()
        val newWebView = mock<WebView>()
        val resultMsg = mock<Message>()
        val newEngineSession = mock<SystemEngineSession>()

        SystemWindowRequest(curWebView, newEngineSession, newWebView, false, false).start()
        verify(resultMsg, never()).sendToTarget()

        SystemWindowRequest(curWebView, newEngineSession, newWebView, false, false, resultMsg).start()
        verify(resultMsg, never()).sendToTarget()

        resultMsg.obj = ""
        SystemWindowRequest(curWebView, newEngineSession, newWebView, false, false, resultMsg).start()
        verify(resultMsg, never()).sendToTarget()

        resultMsg.obj = mock<WebView.WebViewTransport>()
        SystemWindowRequest(curWebView, newEngineSession, newWebView, false, false, resultMsg).start()
        verify(resultMsg, times(1)).sendToTarget()
    }
}
