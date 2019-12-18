/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.webextension.ActionHandler
import mozilla.components.concept.engine.webextension.BrowserAction
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MockWebExtension
import org.mozilla.geckoview.WebExtension
import org.mozilla.geckoview.WebExtensionController

@RunWith(AndroidJUnit4::class)
class GeckoWebExtensionTest {

    @Test
    fun `register background message handler`() {
        val webExtensionController: WebExtensionController = mock()
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        val extension = GeckoWebExtension(
            id = "mozacTest",
            url = "url",
            allowContentMessaging = true,
            supportActions = true,
            webExtensionController = webExtensionController,
            nativeExtension = nativeGeckoWebExt
        )

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
        val webExtensionController: WebExtensionController = mock()
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension(
            id = "mozacTest",
            url = "url",
            allowContentMessaging = true,
            supportActions = true,
            webExtensionController = webExtensionController,
            nativeExtension = nativeGeckoWebExt
        )
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
        val webExtensionController: WebExtensionController = mock()
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()

        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension(
            id = "mozacTest",
            url = "url",
            allowContentMessaging = true,
            supportActions = true,
            webExtensionController = webExtensionController,
            nativeExtension = nativeGeckoWebExt
        )
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
        val webExtensionController: WebExtensionController = mock()
        val nativeGeckoWebExt: WebExtension = mock()
        val messageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val extension = GeckoWebExtension(
            id = "mozacTest",
            url = "url",
            allowContentMessaging = true,
            supportActions = true,
            webExtensionController = webExtensionController,
            nativeExtension = nativeGeckoWebExt
        )
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

    @Test
    fun `register global default action handler`() {
        val webExtensionController: WebExtensionController = mock()
        val nativeGeckoWebExt: WebExtension = mock()
        val actionHandler: ActionHandler = mock()
        val actionDelegateCaptor = argumentCaptor<WebExtension.ActionDelegate>()
        val actionCaptor = argumentCaptor<BrowserAction>()
        val nativeBrowserAction: WebExtension.Action = mock()

        // Verify actions will not be acted on when not supported
        val extensionWithActions = GeckoWebExtension(
            "mozacTest",
            "url",
            false,
            false,
            webExtensionController,
            nativeGeckoWebExt
        )
        extensionWithActions.registerActionHandler(actionHandler)
        verify(nativeGeckoWebExt, never()).setActionDelegate(actionDelegateCaptor.capture())

        // Create extension and register global default action handler
        val extension = GeckoWebExtension(
            id = "mozacTest",
            url = "url",
            allowContentMessaging = true,
            supportActions = true,
            webExtensionController = webExtensionController,
            nativeExtension = nativeGeckoWebExt
        )
        extension.registerActionHandler(actionHandler)
        verify(nativeGeckoWebExt).setActionDelegate(actionDelegateCaptor.capture())

        // Verify that browser actions are forwarded to the handler
        actionDelegateCaptor.value.onBrowserAction(nativeGeckoWebExt, null, nativeBrowserAction)
        verify(actionHandler).onBrowserAction(eq(extension), eq(null), actionCaptor.capture())

        // Verify that toggle popup is forwarded to the handler
        actionDelegateCaptor.value.onTogglePopup(nativeGeckoWebExt, nativeBrowserAction)
        verify(actionHandler).onToggleBrowserActionPopup(eq(extension), actionCaptor.capture())

        // We don't have access to the native WebExtension.Action fields and
        // can't mock them either, but we can verify that we've linked
        // the actions by simulating a click.
        actionCaptor.value.onClick()
        verify(nativeBrowserAction).click()
    }

    @Test
    fun `register session-specific action handler`() {
        val webExtensionController: WebExtensionController = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        whenever(session.geckoSession).thenReturn(geckoSession)

        val nativeGeckoWebExt: WebExtension = mock()
        val actionHandler: ActionHandler = mock()
        val actionDelegateCaptor = argumentCaptor<WebExtension.ActionDelegate>()
        val actionCaptor = argumentCaptor<BrowserAction>()
        val nativeBrowserAction: WebExtension.Action = mock()

        // Create extension and register action handler for session
        val extension = GeckoWebExtension(
            id = "mozacTest",
            url = "url",
            allowContentMessaging = true,
            supportActions = true,
            webExtensionController = webExtensionController,
            nativeExtension = nativeGeckoWebExt
        )
        extension.registerActionHandler(session, actionHandler)
        verify(geckoSession).setWebExtensionActionDelegate(eq(nativeGeckoWebExt), actionDelegateCaptor.capture())

        whenever(geckoSession.getWebExtensionActionDelegate(nativeGeckoWebExt)).thenReturn(actionDelegateCaptor.value)
        assertTrue(extension.hasActionHandler(session))

        // Verify that browser actions are forwarded to the handler
        actionDelegateCaptor.value.onBrowserAction(nativeGeckoWebExt, null, nativeBrowserAction)
        verify(actionHandler).onBrowserAction(eq(extension), eq(session), actionCaptor.capture())

        // We don't have access to the native WebExtension.Action fields and
        // can't mock them either, but we can verify that we've linked
        // the actions by simulating a click.
        actionCaptor.value.onClick()
        verify(nativeBrowserAction).click()
    }

    @Test
    fun `all metadata fields are mapped correctly`() {
        val webExtensionController: WebExtensionController = mock()
        val extensionWithoutMetadata = GeckoWebExtension(
            WebExtension("url", "id", WebExtension.Flags.NONE, webExtensionController),
            webExtensionController
        )
        assertNull(extensionWithoutMetadata.getMetadata())

        val metaDataBundle = GeckoBundle()
        metaDataBundle.putStringArray("permissions", arrayOf("p1", "p2"))
        metaDataBundle.putStringArray("origins", arrayOf("o1", "o2"))
        metaDataBundle.putString("description", "desc")
        metaDataBundle.putString("version", "1.0")
        metaDataBundle.putString("creatorName", "developer1")
        metaDataBundle.putString("creatorURL", "https://developer1.dev")
        metaDataBundle.putString("homepageURL", "https://mozilla.org")
        metaDataBundle.putString("name", "myextension")
        metaDataBundle.putString("optionsPageUrl", "")
        metaDataBundle.putBoolean("openOptionsPageInTab", false)
        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        bundle.putBundle("metaData", metaDataBundle)

        val nativeWebExtension = MockWebExtension(bundle)
        val extensionWithMetadata = GeckoWebExtension(nativeWebExtension, webExtensionController)
        val metadata = extensionWithMetadata.getMetadata()
        assertNotNull(metadata!!)
        assertEquals("1.0", metadata.version)
        assertEquals(listOf("p1", "p2"), metadata.permissions)
        assertEquals(listOf("o1", "o2"), metadata.hostPermissions)
        assertEquals("desc", metadata.description)
        assertEquals("developer1", metadata.developerName)
        assertEquals("https://developer1.dev", metadata.developerUrl)
        assertEquals("https://mozilla.org", metadata.homePageUrl)
        assertEquals("myextension", metadata.name)
        assertNull(metadata.optionsPageUrl)
        assertNull(metadata.openOptionsPageInTab)
    }

    @Test
    fun `nullable metadata fields `() {
        val webExtensionController: WebExtensionController = mock()
        val extensionWithoutMetadata = GeckoWebExtension(
            WebExtension("url", "id", WebExtension.Flags.NONE, webExtensionController),
            webExtensionController
        )
        assertNull(extensionWithoutMetadata.getMetadata())

        val metaDataBundle = GeckoBundle()
        metaDataBundle.putStringArray("permissions", arrayOf("p1", "p2"))
        metaDataBundle.putStringArray("origins", arrayOf("o1", "o2"))
        metaDataBundle.putString("version", "1.0")
        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        bundle.putBundle("metaData", metaDataBundle)

        val nativeWebExtension = MockWebExtension(bundle)
        val extensionWithMetadata = GeckoWebExtension(nativeWebExtension, webExtensionController)
        val metadata = extensionWithMetadata.getMetadata()
        assertNotNull(metadata!!)
        assertEquals("1.0", metadata.version)
        assertEquals(listOf("p1", "p2"), metadata.permissions)
        assertEquals(listOf("o1", "o2"), metadata.hostPermissions)
        assertNull(metadata.description)
        assertNull(metadata.developerName)
        assertNull(metadata.developerUrl)
        assertNull(metadata.homePageUrl)
        assertNull(metadata.name)
        assertNull(metadata.optionsPageUrl)
        assertNull(metadata.openOptionsPageInTab)
    }

    @Test
    fun `isBuiltIn depends on URI scheme`() {
        val webExtensionController: WebExtensionController = mock()
        val builtInExtension = GeckoWebExtension(
            WebExtension("resource://url", "id", WebExtension.Flags.NONE, webExtensionController),
            webExtensionController
        )
        assertTrue(builtInExtension.isBuiltIn())

        val externalExtension = GeckoWebExtension(
            WebExtension("https://url", "id", WebExtension.Flags.NONE, webExtensionController),
            webExtensionController
        )
        assertFalse(externalExtension.isBuiltIn())
    }

    @Test
    fun `isEnabled depends on native state`() {
        val webExtensionController: WebExtensionController = mock()
        val extension = GeckoWebExtension(
            WebExtension("resource://url", "id", WebExtension.Flags.NONE, webExtensionController),
            webExtensionController
        )
        assertTrue(extension.isEnabled())
    }
}
