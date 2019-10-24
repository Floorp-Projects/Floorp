/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.WebExtension

@RunWith(AndroidJUnit4::class)
class GeckoWebExtensionTest {

    @Test
    fun `register background message handler`() {
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        val extension = GeckoWebExtension("mozacTest", "url", true, nativeGeckoWebExt)
        extension.registerBackgroundMessageHandler("mozacTest", messageHandler)

        verify(nativeGeckoWebExt).setMessageDelegate(messageDelegateCaptor.capture(), eq("mozacTest"))

        // Verify messages are forwarded to message handler
        val message: Any = mock()
        val sender: WebExtension.MessageSender = mock()
        whenever(messageHandler.onMessage(eq(message), eq(null))).thenReturn("result")
        assertNotNull(messageDelegateCaptor.value.onMessage("mozacTest", message, sender))
        verify(messageHandler).onMessage(eq(message), eq(null))

        whenever(messageHandler.onMessage(eq(message), eq(null))).thenReturn(null)
        assertNull(messageDelegateCaptor.value.onMessage("mozacTest", message, sender))
        verify(messageHandler, times(2)).onMessage(eq(message), eq(null))

        // Verify port is connected and forwarded to message handler
        val port: WebExtension.Port = mock()
        messageDelegateCaptor.value.onConnect(port)
        verify(messageHandler).onPortConnected(portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)
        assertNotNull(extension.getConnectedPort("mozacTest"))
        assertSame(port, (extension.getConnectedPort("mozacTest") as GeckoPort).nativePort)

        // Verify port messages are forwarded to message handler
        verify(port).setDelegate(portDelegateCaptor.capture())
        val portDelegate = portDelegateCaptor.value
        val portMessage: JSONObject = mock()
        portDelegate.onPortMessage(portMessage, port)
        verify(messageHandler).onPortMessage(eq(portMessage), portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)

        // Verify disconnected port is forwarded to message handler
        portDelegate.onDisconnect(port)
        verify(messageHandler).onPortDisconnected(portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)
        assertNull(extension.getConnectedPort("mozacTest"))
    }

    @Test
    fun `register content message handler`() {
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension("mozacTest", "url", true, nativeGeckoWebExt)
        assertFalse(extension.hasContentMessageHandler(session, "mozacTest"))
        extension.registerContentMessageHandler(session, "mozacTest", messageHandler)
        verify(geckoSession).setMessageDelegate(eq(nativeGeckoWebExt), messageDelegateCaptor.capture(), eq("mozacTest"))

        // Verify messages are forwarded to message handler and return value passed on
        val message: Any = mock()
        val sender: WebExtension.MessageSender = mock()
        whenever(messageHandler.onMessage(eq(message), eq(session))).thenReturn("result")
        assertNotNull(messageDelegateCaptor.value.onMessage("mozacTest", message, sender))
        verify(messageHandler).onMessage(eq(message), eq(session))

        whenever(messageHandler.onMessage(eq(message), eq(session))).thenReturn(null)
        assertNull(messageDelegateCaptor.value.onMessage("mozacTest", message, sender))
        verify(messageHandler, times(2)).onMessage(eq(message), eq(session))

        // Verify port is connected and forwarded to message handler
        val port: WebExtension.Port = mock()
        messageDelegateCaptor.value.onConnect(port)
        verify(messageHandler).onPortConnected(portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)
        assertSame(session, (portCaptor.value as GeckoPort).engineSession)
        assertNotNull(extension.getConnectedPort("mozacTest", session))
        assertSame(port, (extension.getConnectedPort("mozacTest", session) as GeckoPort).nativePort)

        // Verify port messages are forwarded to message handler
        verify(port).setDelegate(portDelegateCaptor.capture())
        val portDelegate = portDelegateCaptor.value
        val portMessage: JSONObject = mock()
        portDelegate.onPortMessage(portMessage, port)
        verify(messageHandler).onPortMessage(eq(portMessage), portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)
        assertSame(session, (portCaptor.value as GeckoPort).engineSession)

        // Verify disconnected port is forwarded to message handler
        portDelegate.onDisconnect(port)
        verify(messageHandler).onPortDisconnected(portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)
        assertSame(session, (portCaptor.value as GeckoPort).engineSession)
        assertNull(extension.getConnectedPort("mozacTest", session))
    }

    @Test
    fun `disconnect port from content script`() {
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()

        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension("mozacTest", "url", true, nativeGeckoWebExt)
        extension.registerContentMessageHandler(session, "mozacTest", messageHandler)
        verify(geckoSession).setMessageDelegate(eq(nativeGeckoWebExt), messageDelegateCaptor.capture(), eq("mozacTest"))

        // Connect port
        val port: WebExtension.Port = mock()
        messageDelegateCaptor.value.onConnect(port)
        assertNotNull(extension.getConnectedPort("mozacTest", session))

        // Disconnect port
        extension.disconnectPort("mozacTest", session)
        verify(port).disconnect()
        assertNull(extension.getConnectedPort("mozacTest", session))
    }

    @Test
    fun `disconnect port from background script`() {
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val extension = GeckoWebExtension("mozacTest", "url", true, nativeGeckoWebExt)
        extension.registerBackgroundMessageHandler("mozacTest", messageHandler)

        verify(nativeGeckoWebExt).setMessageDelegate(messageDelegateCaptor.capture(), eq("mozacTest"))

        // Connect port
        val port: WebExtension.Port = mock()
        messageDelegateCaptor.value.onConnect(port)
        assertNotNull(extension.getConnectedPort("mozacTest"))

        // Disconnect port
        extension.disconnectPort("mozacTest")
        verify(port).disconnect()
        assertNull(extension.getConnectedPort("mozacTest"))
    }
}