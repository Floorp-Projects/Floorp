/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webextension

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
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
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.MockCreateTabDetails
import org.mozilla.geckoview.MockUpdateTabDetails
import org.mozilla.geckoview.MockWebExtension
import org.mozilla.geckoview.WebExtension
import org.mozilla.geckoview.WebExtensionController

@RunWith(AndroidJUnit4::class)
class GeckoWebExtensionTest {

    @Test
    fun `register background message handler`() {
        val runtime: GeckoRuntime = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeExtension()
        val messageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        val extension = GeckoWebExtension(
            runtime = runtime,
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
        val runtime: GeckoRuntime = mock()
        val webExtensionSessionController: WebExtension.SessionController = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeExtension()
        val messageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val portCaptor = argumentCaptor<Port>()
        val portDelegateCaptor = argumentCaptor<WebExtension.PortDelegate>()

        whenever(geckoSession.webExtensionController).thenReturn(webExtensionSessionController)
        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt
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

        // Verify disconnected port is forwarded to message handler
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
        val nativeGeckoWebExt: WebExtension = mockNativeExtension()
        val messageHandler: MessageHandler = mock()
        val session: GeckoEngineSession = mock()
        val geckoSession: GeckoSession = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()

        whenever(geckoSession.webExtensionController).thenReturn(webExtensionSessionController)
        whenever(session.geckoSession).thenReturn(geckoSession)

        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt
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
        val nativeGeckoWebExt: WebExtension = mockNativeExtension()
        val messageHandler: MessageHandler = mock()
        val messageDelegateCaptor = argumentCaptor<WebExtension.MessageDelegate>()
        val extension = GeckoWebExtension(
            runtime = runtime,
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
        val runtime: GeckoRuntime = mock()
        val nativeGeckoWebExt: WebExtension = mockNativeExtension()
        val actionHandler: ActionHandler = mock()
        val actionDelegateCaptor = argumentCaptor<WebExtension.ActionDelegate>()
        val browserActionCaptor = argumentCaptor<Action>()
        val pageActionCaptor = argumentCaptor<Action>()
        val nativeBrowserAction: WebExtension.Action = mock()
        val nativePageAction: WebExtension.Action = mock()

        // Create extension and register global default action handler
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt
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

        val nativeGeckoWebExt: WebExtension = mockNativeExtension()
        val actionHandler: ActionHandler = mock()
        val actionDelegateCaptor = argumentCaptor<WebExtension.ActionDelegate>()
        val browserActionCaptor = argumentCaptor<Action>()
        val pageActionCaptor = argumentCaptor<Action>()
        val nativeBrowserAction: WebExtension.Action = mock()
        val nativePageAction: WebExtension.Action = mock()

        // Create extension and register action handler for session
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt
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
        var metaDataBundle = GeckoBundle()
        metaDataBundle.putStringArray("disabledFlags", emptyArray())
        metaDataBundle.putString("optionsPageUrl", "optionsPageUrl")

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        bundle.putBundle("metaData", metaDataBundle)

        val nativeGeckoWebExt: WebExtension = mockNativeExtension(bundle)

        // Create extension and register global tab handler
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt
        )
        extension.registerTabHandler(tabHandler)
        verify(nativeGeckoWebExt).tabDelegate = tabDelegateCaptor.capture()

        // Verify that tab methods are forwarded to the handler
        val tabBundle = GeckoBundle()
        tabBundle.putBoolean("active", true)
        tabBundle.putString("url", "url")
        val tabDetails = MockCreateTabDetails(tabBundle)
        tabDelegateCaptor.value.onNewTab(nativeGeckoWebExt, tabDetails)
        verify(tabHandler).onNewTab(eq(extension), engineSessionCaptor.capture(), eq(true), eq("url"))
        assertNotNull(engineSessionCaptor.value)

        tabDelegateCaptor.value.onOpenOptionsPage(nativeGeckoWebExt)
        verify(tabHandler, never()).onNewTab(eq(extension), any(), eq(false), eq("http://options-page.moz"))

        metaDataBundle = GeckoBundle()
        metaDataBundle.putStringArray("disabledFlags", emptyArray())
        bundle.putBundle("metaData", metaDataBundle)
        val nativeGeckoWebExtWithMetadata = MockWebExtension(bundle)
        tabDelegateCaptor.value.onOpenOptionsPage(nativeGeckoWebExtWithMetadata)
        verify(tabHandler, never()).onNewTab(eq(extension), any(), eq(false), eq("http://options-page.moz"))

        metaDataBundle.putString("optionsPageURL", "http://options-page.moz")
        bundle.putBundle("metaData", metaDataBundle)
        val nativeGeckoWebExtWithOptionsPageUrl = MockWebExtension(bundle)
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

        val nativeGeckoWebExt: WebExtension = mockNativeExtension()
        // Create extension and register tab handler for session
        val extension = GeckoWebExtension(
            runtime = runtime,
            nativeExtension = nativeGeckoWebExt
        )
        extension.registerTabHandler(session, tabHandler)
        verify(webExtensionSessionController).setTabDelegate(eq(nativeGeckoWebExt), tabDelegateCaptor.capture())

        assertFalse(extension.hasTabHandler(session))
        whenever(webExtensionSessionController.getTabDelegate(nativeGeckoWebExt)).thenReturn(tabDelegateCaptor.value)
        assertTrue(extension.hasTabHandler(session))

        // Verify that tab methods are forwarded to the handler
        val tabBundle = GeckoBundle()
        tabBundle.putBoolean("active", true)
        val tabDetails = MockUpdateTabDetails(tabBundle)
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

        val metaDataBundle = GeckoBundle()
        metaDataBundle.putStringArray("promptPermissions", arrayOf("p1", "p2"))
        metaDataBundle.putStringArray("origins", arrayOf("o1", "o2"))
        metaDataBundle.putString("description", "desc")
        metaDataBundle.putString("version", "1.0")
        metaDataBundle.putString("creatorName", "developer1")
        metaDataBundle.putString("creatorURL", "https://developer1.dev")
        metaDataBundle.putString("homepageURL", "https://mozilla.org")
        metaDataBundle.putString("name", "myextension")
        metaDataBundle.putString("optionsPageURL", "http://options-page.moz")
        metaDataBundle.putString("baseURL", "moz-extension://123c5c5b-cd03-4bea-b23f-ac0b9ab40257/")
        metaDataBundle.putBoolean("openOptionsPageInTab", false)
        metaDataBundle.putStringArray("disabledFlags", arrayOf("userDisabled"))
        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        bundle.putBundle("metaData", metaDataBundle)

        val nativeWebExtension = MockWebExtension(bundle)
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
        assertTrue(metadata.disabledFlags.contains(DisabledFlags.USER))
        assertFalse(metadata.disabledFlags.contains(DisabledFlags.BLOCKLIST))
        assertFalse(metadata.disabledFlags.contains(DisabledFlags.APP_SUPPORT))
    }

    @Test
    fun `nullable metadata fields`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val metaDataBundle = GeckoBundle()
        metaDataBundle.putStringArray("promptPermissions", arrayOf("p1", "p2"))
        metaDataBundle.putString("version", "1.0")
        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        metaDataBundle.putStringArray("disabledFlags", emptyArray())
        metaDataBundle.putString("baseURL", "moz-extension://123c5c5b-cd03-4bea-b23f-ac0b9ab40257/")
        bundle.putBundle("metaData", metaDataBundle)

        val nativeWebExtension = MockWebExtension(bundle)
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

        val builtInBundle = GeckoBundle()
        builtInBundle.putBoolean("isBuiltIn", true)
        builtInBundle.putString("webExtensionId", "id")
        builtInBundle.putString("locationURI", "uri")
        val builtInExtension = GeckoWebExtension(
            mockNativeExtension(builtInBundle),
            runtime
        )
        assertTrue(builtInExtension.isBuiltIn())

        val externalBundle = GeckoBundle()
        externalBundle.putBoolean("isBuiltIn", false)
        externalBundle.putString("webExtensionId", "id")
        externalBundle.putString("locationURI", "uri")
        val externalExtension = GeckoWebExtension(
            mockNativeExtension(externalBundle),
            runtime
        )
        assertFalse(externalExtension.isBuiltIn())
    }

    @Test
    fun `isEnabled depends on native state and defaults to true if state unknown`() {
        val runtime: GeckoRuntime = mock()
        whenever(runtime.webExtensionController).thenReturn(mock())
        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")

        val metaDataBundle = GeckoBundle()
        metaDataBundle.putBoolean("enabled", true)
        metaDataBundle.putStringArray("disabledFlags", emptyArray())
        bundle.putBundle("metaData", metaDataBundle)
        val nativeEnabledWebExtension = MockWebExtension(bundle)
        val enabledWebExtension = GeckoWebExtension(nativeEnabledWebExtension, runtime)
        assertTrue(enabledWebExtension.isEnabled())

        metaDataBundle.putBoolean("enabled", false)
        metaDataBundle.putStringArray("disabledFlags", arrayOf("userDisabled"))
        bundle.putBundle("metaData", metaDataBundle)
        val nativeDisabledWebExtnesion = MockWebExtension(bundle)
        val disabledWebExtension = GeckoWebExtension(nativeDisabledWebExtnesion, runtime)
        assertFalse(disabledWebExtension.isEnabled())
    }

    @Test
    fun `isAllowedInPrivateBrowsing depends on native state and defaults to false if state unknown`() {
        val runtime: GeckoRuntime = mock()
        whenever(runtime.webExtensionController).thenReturn(mock())

        val builtInBundle = GeckoBundle()
        builtInBundle.putBoolean("isBuiltIn", true)
        builtInBundle.putString("webExtensionId", "id")
        builtInBundle.putString("locationURI", "uri")
        builtInBundle.putBoolean("privateBrowsingAllowed", false)

        val builtInExtension = GeckoWebExtension(
            mockNativeExtension(builtInBundle),
            runtime
        )
        assertTrue(builtInExtension.isAllowedInPrivateBrowsing())

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")

        val metaDataBundle = GeckoBundle()
        metaDataBundle.putBoolean("privateBrowsingAllowed", true)
        metaDataBundle.putStringArray("disabledFlags", emptyArray())
        bundle.putBundle("metaData", metaDataBundle)
        val nativeWebExtensionWithPrivateBrowsing = MockWebExtension(bundle)
        val webExtensionWithPrivateBrowsing = GeckoWebExtension(nativeWebExtensionWithPrivateBrowsing, runtime)
        assertTrue(webExtensionWithPrivateBrowsing.isAllowedInPrivateBrowsing())

        metaDataBundle.putBoolean("privateBrowsingAllowed", false)
        metaDataBundle.putStringArray("disabledFlags", emptyArray())
        bundle.putBundle("metaData", metaDataBundle)
        val nativeWebExtensionWithoutPrivateBrowsing = MockWebExtension(bundle)
        val webExtensionWithoutPrivateBrowsing = GeckoWebExtension(nativeWebExtensionWithoutPrivateBrowsing, runtime)
        assertFalse(webExtensionWithoutPrivateBrowsing.isAllowedInPrivateBrowsing())
    }

    private fun mockNativeExtension(useBundle: GeckoBundle? = null): WebExtension {
        val bundle = useBundle ?: GeckoBundle().apply {
            putString("webExtensionId", "id")
            putString("locationURI", "uri")
        }
        return spy(MockWebExtension(bundle))
    }
}
