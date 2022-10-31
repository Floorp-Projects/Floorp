/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.engine.webextension.ActionHandler
import mozilla.components.concept.engine.webextension.DisabledFlags
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.TabHandler
import mozilla.components.support.test.any
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
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.Image
import org.mozilla.geckoview.WebExtension
import org.mozilla.geckoview.WebExtensionController

@RunWith(AndroidJUnit4::class)
class GeckoWebExtensionTest {

    @Test
    fun `register background message handler`() {
        val runtime: GeckoRuntime = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeWebExtension()
        val messageHandler: MessageHandler = mock()
        val updatedMessageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
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

        // Verify content message handler can be updated and receive messages
        extension.registerBackgroundMessageHandler("mozacTest", updatedMessageHandler)
        verify(port, times(2)).setDelegate(portDelegateCaptor.capture())
        portDelegateCaptor.value.onPortMessage(portMessage, port)
        verify(updatedMessageHandler).onPortMessage(eq(portMessage), portCaptor.capture())

        // Verify disconnected port is forwarded to message handler if connected
        portDelegate.onDisconnect(mock())
        verify(messageHandler, never()).onPortDisconnected(portCaptor.capture())

        portDelegate.onDisconnect(port)
        verify(messageHandler).onPortDisconnected(portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)
        assertNull(extension.getConnectedPort("mozacTest"))
    }

    @Test
    fun `register content message handler`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionSessionController: WebExtension.SessionController = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeWebExtension()
        val messageHandler: MessageHandler = mock()
        val updatedMessageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        whenever(geckoSession.webExtensionController).thenReturn(webExtensionSessionController)
        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
        )
        assertFalse(extension.hasContentMessageHandler(session, "mozacTest"))
        extension.registerContentMessageHandler(session, "mozacTest", messageHandler)
        verify(webExtensionSessionController).setMessageDelegate(eq(nativeGeckoWebExt), messageDelegateCaptor.capture(), eq("mozacTest"))

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

        // Verify content message handler can be updated and receive messages
        extension.registerContentMessageHandler(session, "mozacTest", updatedMessageHandler)
        verify(port, times(2)).setDelegate(portDelegateCaptor.capture())
        portDelegateCaptor.value.onPortMessage(portMessage, port)
        verify(updatedMessageHandler).onPortMessage(eq(portMessage), portCaptor.capture())

        // Verify disconnected port is forwarded to message handler if connected
        portDelegate.onDisconnect(mock())
        verify(messageHandler, never()).onPortDisconnected(portCaptor.capture())

        portDelegate.onDisconnect(port)
        verify(messageHandler).onPortDisconnected(portCaptor.capture())
        assertSame(port, (portCaptor.value as GeckoPort).nativePort)
        assertSame(session, (portCaptor.value as GeckoPort).engineSession)
        assertNull(extension.getConnectedPort("mozacTest", session))
    }

    @Test
    fun `disconnect port from content script`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionSessionController: WebExtension.SessionController = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeWebExtension()
        val messageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()

        whenever(geckoSession.webExtensionController).thenReturn(webExtensionSessionController)
        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
        )
        extension.registerContentMessageHandler(session, "mozacTest", messageHandler)
        verify(webExtensionSessionController).setMessageDelegate(eq(nativeGeckoWebExt), messageDelegateCaptor.capture(), eq("mozacTest"))

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
        val runtime: GeckoRuntime = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeWebExtension()
        val messageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
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
        val runtime: GeckoRuntime = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeWebExtension()
        val actionHandler: ActionHandler = mock()
        val actionDelegateCaptor = argumentCaptor<WebExtension.ActionDelegate>()
        val browserActionCaptor = argumentCaptor<Action>()
        val pageActionCaptor = argumentCaptor<Action>()
        val nativeBrowserAction: WebExtension.Action = mock()
        val nativePageAction: WebExtension.Action = mock()

        // Create extension and register global default action handler
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
        )
        extension.registerActionHandler(actionHandler)
        verify(nativeGeckoWebExt).setActionDelegate(actionDelegateCaptor.capture())

        // Verify that browser actions are forwarded to the handler
        actionDelegateCaptor.value.onBrowserAction(nativeGeckoWebExt, null, nativeBrowserAction)
        verify(actionHandler).onBrowserAction(eq(extension), eq(null), browserActionCaptor.capture())

        // Verify that page actions are forwarded to the handler
        actionDelegateCaptor.value.onPageAction(nativeGeckoWebExt, null, nativePageAction)
        verify(actionHandler).onPageAction(eq(extension), eq(null), pageActionCaptor.capture())

        // Verify that toggle popup is forwarded to the handler
        actionDelegateCaptor.value.onTogglePopup(nativeGeckoWebExt, nativeBrowserAction)
        verify(actionHandler).onToggleActionPopup(eq(extension), any())

        // We don't have access to the native WebExtension.Action fields and
        // can't mock them either, but we can verify that we've linked
        // the actions by simulating a click.
        browserActionCaptor.value.onClick()
        verify(nativeBrowserAction).click()
        pageActionCaptor.value.onClick()
        verify(nativePageAction).click()
    }

    @Test
    fun `register session-specific action handler`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionSessionController: WebExtension.SessionController = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        whenever(geckoSession.webExtensionController).thenReturn(webExtensionSessionController)
        whenever(session.geckoSession).thenReturn(geckoSession)

        val nativeGeckoWebExt: WebExtension = mockNativeWebExtension()
        val actionHandler: ActionHandler = mock()
        val actionDelegateCaptor = argumentCaptor<WebExtension.ActionDelegate>()
        val browserActionCaptor = argumentCaptor<Action>()
        val pageActionCaptor = argumentCaptor<Action>()
        val nativeBrowserAction: WebExtension.Action = mock()
        val nativePageAction: WebExtension.Action = mock()

        // Create extension and register action handler for session
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
        )
        extension.registerActionHandler(session, actionHandler)
        verify(webExtensionSessionController).setActionDelegate(eq(nativeGeckoWebExt), actionDelegateCaptor.capture())

        whenever(webExtensionSessionController.getActionDelegate(nativeGeckoWebExt)).thenReturn(actionDelegateCaptor.value)
        assertTrue(extension.hasActionHandler(session))

        // Verify that browser actions are forwarded to the handler
        actionDelegateCaptor.value.onBrowserAction(nativeGeckoWebExt, null, nativeBrowserAction)
        verify(actionHandler).onBrowserAction(eq(extension), eq(session), browserActionCaptor.capture())

        // Verify that page actions are forwarded to the handler
        actionDelegateCaptor.value.onPageAction(nativeGeckoWebExt, null, nativePageAction)
        verify(actionHandler).onPageAction(eq(extension), eq(session), pageActionCaptor.capture())

        // We don't have access to the native WebExtension.Action fields and
        // can't mock them either, but we can verify that we've linked
        // the actions by simulating a click.
        browserActionCaptor.value.onClick()
        verify(nativeBrowserAction).click()
        pageActionCaptor.value.onClick()
        verify(nativePageAction).click()
    }

    @Test
    fun `register global tab handler`() {
        val runtime: GeckoRuntime = mock()
        whenever(runtime.settings).thenReturn(mock())
        whenever(runtime.webExtensionController).thenReturn(mock())
        val tabHandler: TabHandler = mock()
        val tabDelegateCaptor = argumentCaptor<WebExtension.TabDelegate>()
        val engineSessionCaptor = argumentCaptor<GeckoEngineSession>()

        val nativeGeckoWebExt: WebExtension =
            mockNativeWebExtension(id = "id", location = "uri", metaData = mockNativeWebExtensionMetaData())

        // Create extension and register global tab handler
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
        )
        val defaultSettings: DefaultSettings = mock()

        extension.registerTabHandler(tabHandler, defaultSettings)
        verify(nativeGeckoWebExt).tabDelegate = tabDelegateCaptor.capture()

        // Verify that tab methods are forwarded to the handler
        val tabDetails = mockCreateTabDetails(active = true, url = "url")
        tabDelegateCaptor.value.onNewTab(nativeGeckoWebExt, tabDetails)
        verify(tabHandler).onNewTab(eq(extension), engineSessionCaptor.capture(), eq(true), eq("url"))
        assertNotNull(engineSessionCaptor.value)

        tabDelegateCaptor.value.onOpenOptionsPage(nativeGeckoWebExt)
        verify(tabHandler, never()).onNewTab(eq(extension), any(), eq(false), eq("http://options-page.moz"))

        val nativeGeckoWebExtWithMetadata =
            mockNativeWebExtension(id = "id", location = "uri", metaData = mockNativeWebExtensionMetaData())
        tabDelegateCaptor.value.onOpenOptionsPage(nativeGeckoWebExtWithMetadata)
        verify(tabHandler, never()).onNewTab(eq(extension), any(), eq(false), eq("http://options-page.moz"))

        val nativeGeckoWebExtWithOptionsPageUrl = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(optionsPageUrl = "http://options-page.moz"),
        )
        tabDelegateCaptor.value.onOpenOptionsPage(nativeGeckoWebExtWithOptionsPageUrl)
        verify(tabHandler).onNewTab(eq(extension), any(), eq(false), eq("http://options-page.moz"))
    }

    @Test
    fun `register session-specific tab handler`() {
        val runtime: GeckoRuntime = mock()
        whenever(runtime.webExtensionController).thenReturn(mock())
        val webExtensionSessionController: WebExtension.SessionController = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        whenever(geckoSession.webExtensionController).thenReturn(webExtensionSessionController)
        whenever(session.geckoSession).thenReturn(geckoSession)

        val tabHandler: TabHandler = mock()
        val tabDelegateCaptor = argumentCaptor<WebExtension.SessionTabDelegate>()

        val nativeGeckoWebExt: WebExtension = mockNativeWebExtension()
        // Create extension and register tab handler for session
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt,
        )
        extension.registerTabHandler(session, tabHandler)
        verify(webExtensionSessionController).setTabDelegate(eq(nativeGeckoWebExt), tabDelegateCaptor.capture())

        assertFalse(extension.hasTabHandler(session))
        whenever(webExtensionSessionController.getTabDelegate(nativeGeckoWebExt)).thenReturn(tabDelegateCaptor.value)
        assertTrue(extension.hasTabHandler(session))

        // Verify that tab methods are forwarded to the handler
        val tabDetails = mockUpdateTabDetails(active = true)
        tabDelegateCaptor.value.onUpdateTab(nativeGeckoWebExt, mock(), tabDetails)
        verify(tabHandler).onUpdateTab(eq(extension), eq(session), eq(true), eq(null))

        tabDelegateCaptor.value.onCloseTab(nativeGeckoWebExt, mock())
        verify(tabHandler).onCloseTab(eq(extension), eq(session))
    }

    @Test
    fun `all metadata fields are mapped correctly`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val nativeWebExtension = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(
                origins = arrayOf("o1", "o2"),
                description = "desc",
                version = "1.0",
                creatorName = "developer1",
                creatorUrl = "https://developer1.dev",
                homepageUrl = "https://mozilla.org",
                name = "myextension",
                optionsPageUrl = "http://options-page.moz",
                baseUrl = "moz-extension://123c5c5b-cd03-4bea-b23f-ac0b9ab40257/",
                openOptionsPageInTab = false,
                disabledFlags = DisabledFlags.USER,
                temporary = true,
                permissions = arrayOf("p1", "p2"),
            ),
        )
        val extensionWithMetadata = GeckoWebExtension(nativeWebExtension, runtime)
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
        assertEquals("http://options-page.moz", metadata.optionsPageUrl)
        assertEquals("moz-extension://123c5c5b-cd03-4bea-b23f-ac0b9ab40257/", metadata.baseUrl)
        assertFalse(metadata.openOptionsPageInTab)
        assertTrue(metadata.temporary)
        assertTrue(metadata.disabledFlags.contains(DisabledFlags.USER))
        assertFalse(metadata.disabledFlags.contains(DisabledFlags.BLOCKLIST))
        assertFalse(metadata.disabledFlags.contains(DisabledFlags.APP_SUPPORT))
    }

    @Test
    fun `nullable metadata fields`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val nativeWebExtension = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(
                version = "1.0",
                baseUrl = "moz-extension://123c5c5b-cd03-4bea-b23f-ac0b9ab40257/",
                disabledFlags = DisabledFlags.USER,
                permissions = arrayOf("p1", "p2"),
            ),
        )
        val extensionWithMetadata = GeckoWebExtension(nativeWebExtension, runtime)
        val metadata = extensionWithMetadata.getMetadata()
        assertNotNull(metadata!!)
        assertEquals("1.0", metadata.version)
        assertEquals(listOf("p1", "p2"), metadata.permissions)
        assertEquals(emptyList<String>(), metadata.hostPermissions)
        assertEquals("moz-extension://123c5c5b-cd03-4bea-b23f-ac0b9ab40257/", metadata.baseUrl)
        assertNull(metadata.description)
        assertNull(metadata.developerName)
        assertNull(metadata.developerUrl)
        assertNull(metadata.homePageUrl)
        assertNull(metadata.name)
        assertNull(metadata.optionsPageUrl)
    }

    @Test
    fun `isBuiltIn depends on native state`() {
        val runtime: GeckoRuntime = mock()

        val builtInExtension = GeckoWebExtension(
            mockNativeWebExtension(id = "id", location = "uri", isBuiltIn = true),
            runtime,
        )
        assertTrue(builtInExtension.isBuiltIn())

        val externalExtension = GeckoWebExtension(
            mockNativeWebExtension(id = "id", location = "uri", isBuiltIn = false),
            runtime,
        )
        assertFalse(externalExtension.isBuiltIn())
    }

    @Test
    fun `isEnabled depends on native state and defaults to true if state unknown`() {
        val runtime: GeckoRuntime = mock()
        whenever(runtime.webExtensionController).thenReturn(mock())

        val nativeEnabledWebExtension = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(
                enabled = true,
            ),
        )
        val enabledWebExtension = GeckoWebExtension(nativeEnabledWebExtension, runtime)
        assertTrue(enabledWebExtension.isEnabled())

        val nativeDisabledWebExtension = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(
                enabled = false,
            ),
        )
        val disabledWebExtension = GeckoWebExtension(nativeDisabledWebExtension, runtime)
        assertFalse(disabledWebExtension.isEnabled())
    }

    @Test
    fun `isAllowedInPrivateBrowsing depends on native state and defaults to false if state unknown`() {
        val runtime: GeckoRuntime = mock()
        whenever(runtime.webExtensionController).thenReturn(mock())

        val nativeBuiltInExtension = mockNativeWebExtension(
            id = "id",
            location = "uri",
            isBuiltIn = true,
            metaData = mockNativeWebExtensionMetaData(
                allowedInPrivateBrowsing = false,
            ),
        )
        val builtInExtension = GeckoWebExtension(nativeBuiltInExtension, runtime)
        assertTrue(builtInExtension.isAllowedInPrivateBrowsing())

        val nativeWebExtensionWithPrivateBrowsing = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(
                allowedInPrivateBrowsing = true,
            ),
        )
        val webExtensionWithPrivateBrowsing = GeckoWebExtension(nativeWebExtensionWithPrivateBrowsing, runtime)
        assertTrue(webExtensionWithPrivateBrowsing.isAllowedInPrivateBrowsing())

        val nativeWebExtensionWithoutPrivateBrowsing = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(
                allowedInPrivateBrowsing = false,
            ),
        )
        val webExtensionWithoutPrivateBrowsing = GeckoWebExtension(nativeWebExtensionWithoutPrivateBrowsing, runtime)
        assertFalse(webExtensionWithoutPrivateBrowsing.isAllowedInPrivateBrowsing())
    }

    @Test
    fun `loadIcon tries to load icon from metadata`() {
        val runtime: GeckoRuntime = mock()
        whenever(runtime.webExtensionController).thenReturn(mock())

        val iconMock: Image = mock()
        whenever(iconMock.getBitmap(48)).thenReturn(mock())
        val nativeWebExtensionWithIcon = mockNativeWebExtension(
            id = "id",
            location = "uri",
            metaData = mockNativeWebExtensionMetaData(icon = iconMock),
        )

        val webExtensionWithIcon = GeckoWebExtension(nativeWebExtensionWithIcon, runtime)
        webExtensionWithIcon.getIcon(48)
        verify(iconMock).getBitmap(48)
    }
}
