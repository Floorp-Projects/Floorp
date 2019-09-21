/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.mock
import org.json.JSONObject
import org.junit.Assert.assertSame
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
    fun `port holds engine session`() {
        val engineSession: EngineSession = mock()
        val port = object : Port(engineSession) {
            override fun name(): String {
                return "test"
            }

            override fun disconnect() {}

            override fun postMessage(message: JSONObject) { }
        }

        assertSame(engineSession, port.engineSession)
    }
}
