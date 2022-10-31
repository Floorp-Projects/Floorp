/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class WebExtensionTest {

    @Test
    fun `message handler has default methods`() {
        val messageHandler = object : MessageHandler {}

        messageHandler.onPortConnected(mock())
        messageHandler.onPortDisconnected(mock())
        messageHandler.onPortMessage(mock(), mock())
        messageHandler.onMessage(mock(), mock())
    }

    @Test
    fun `tab handler has default methods`() {
        val tabHandler = object : TabHandler {}

        tabHandler.onUpdateTab(mock(), mock(), false, "")
        tabHandler.onCloseTab(mock(), mock())
        tabHandler.onNewTab(mock(), mock(), false, "")
    }

    @Test
    fun `action handler has default methods`() {
        val actionHandler = object : ActionHandler {}

        actionHandler.onPageAction(mock(), mock(), mock())
        actionHandler.onBrowserAction(mock(), mock(), mock())
        actionHandler.onToggleActionPopup(mock(), mock())
    }

    @Test
    fun `port holds engine session`() {
        val engineSession: EngineSession = mock()
        val port = object : Port(engineSession) {
            override fun name(): String {
                return "test"
            }

            override fun disconnect() {}

            override fun senderUrl(): String {
                return "https://foo.bar"
            }

            override fun postMessage(message: JSONObject) { }
        }

        assertSame(engineSession, port.engineSession)
    }

    @Test
    fun `unsupported check`() {
        val extension: WebExtension = mock()
        assertFalse(extension.isUnsupported())

        val metadata: Metadata = mock()
        whenever(extension.getMetadata()).thenReturn(metadata)
        assertFalse(extension.isUnsupported())

        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(DisabledFlags.BLOCKLIST))
        assertFalse(extension.isUnsupported())

        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(DisabledFlags.APP_SUPPORT))
        assertTrue(extension.isUnsupported())
    }
}
