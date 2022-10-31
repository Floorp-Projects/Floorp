/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class WebExtensionControllerTest {
    private val extensionId = "test-id"
    private val defaultPort = "test-messaging-id"
    private val extensionUrl = "test-url"

    @Before
    fun setup() {
        WebExtensionController.installedExtensions.clear()
    }

    @Test
    fun `install webextension - installs and invokes success and error callbacks`() {
        val engine: Engine = mock()
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)

        var onSuccessInvoked = false
        var onErrorInvoked = false
        controller.install(engine, onSuccess = { onSuccessInvoked = true }, onError = { onErrorInvoked = true })

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
            eq(extensionId),
            eq(extensionUrl),
            onSuccess.capture(),
            onError.capture(),
        )
        assertFalse(WebExtensionController.installedExtensions.containsKey(extensionId))

        onSuccess.value.invoke(mock())
        assertTrue(onSuccessInvoked)
        assertFalse(onErrorInvoked)
        assertTrue(WebExtensionController.installedExtensions.containsKey(extensionId))

        controller.install(engine)
        verify(engine, times(1)).installWebExtension(
            eq(extensionId),
            eq(extensionUrl),
            onSuccess.capture(),
            onError.capture(),
        )

        onError.value.invoke("", mock())
        assertTrue(onErrorInvoked)
    }

    @Test
    fun `install webextension - invokes success callback if extension already installed`() {
        val engine: Engine = mock()
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)

        var onSuccessInvoked = false
        var onErrorInvoked = false
        WebExtensionController.installedExtensions[extensionId] = mock()
        controller.install(engine, onSuccess = { onSuccessInvoked = true }, onError = { onErrorInvoked = true })

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, never()).installWebExtension(
            eq(extensionId),
            eq(extensionUrl),
            onSuccess.capture(),
            onError.capture(),
        )
        assertTrue(onSuccessInvoked)
        assertFalse(onErrorInvoked)
    }

    @Test
    fun `register content message handler if extension installed`() {
        val extension: WebExtension = mock()
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)
        WebExtensionController.installedExtensions[extensionId] = extension

        val session: EngineSession = mock()
        val messageHandler: MessageHandler = mock()
        controller.registerContentMessageHandler(session, messageHandler)
        verify(extension).registerContentMessageHandler(session, defaultPort, messageHandler)
    }

    @Test
    fun `register content message handler before extension is installed`() {
        val engine: Engine = mock()
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)
        controller.install(engine)

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
            eq(extensionId),
            eq(extensionUrl),
            onSuccess.capture(),
            onError.capture(),
        )

        val session: EngineSession = mock()
        val messageHandler: MessageHandler = mock()
        controller.registerContentMessageHandler(session, messageHandler)

        val extension: WebExtension = mock()
        onSuccess.value.invoke(extension)
        verify(extension).registerContentMessageHandler(session, defaultPort, messageHandler)
    }

    @Test
    fun `send content message`() {
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)

        val message: JSONObject = mock()
        val extension: WebExtension = mock()
        val session: EngineSession = mock()
        val port: Port = mock()
        whenever(extension.getConnectedPort(defaultPort, session)).thenReturn(port)

        controller.sendContentMessage(message, null)
        verify(port, never()).postMessage(message)

        controller.sendContentMessage(message, session)
        verify(port, never()).postMessage(message)

        WebExtensionController.installedExtensions[extensionId] = extension

        controller.sendContentMessage(message, session)
        verify(port, times(1)).postMessage(message)
    }

    @Test
    fun `register background message handler if extension installed`() {
        val extension: WebExtension = mock()
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)
        WebExtensionController.installedExtensions[extensionId] = extension

        val messageHandler: MessageHandler = mock()
        controller.registerBackgroundMessageHandler(messageHandler)
        verify(extension).registerBackgroundMessageHandler(defaultPort, messageHandler)
    }

    @Test
    fun `register background message handler before extension is installed`() {
        val engine: Engine = mock()
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)
        controller.install(engine)

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((String, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(
            eq(extensionId),
            eq(extensionUrl),
            onSuccess.capture(),
            onError.capture(),
        )

        val messageHandler: MessageHandler = mock()
        controller.registerBackgroundMessageHandler(messageHandler)

        val extension: WebExtension = mock()
        onSuccess.value.invoke(extension)
        verify(extension).registerBackgroundMessageHandler(defaultPort, messageHandler)
    }

    @Test
    fun `send background message`() {
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)

        val message: JSONObject = mock()
        val extension: WebExtension = mock()
        val port: Port = mock()
        whenever(extension.getConnectedPort(defaultPort)).thenReturn(port)

        controller.sendBackgroundMessage(message)
        verify(port, never()).postMessage(message)

        WebExtensionController.installedExtensions[extensionId] = extension

        controller.sendBackgroundMessage(message)
        verify(port, times(1)).postMessage(message)
    }

    @Test
    fun `check if port connected`() {
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)

        val extension: WebExtension = mock()
        val session: EngineSession = mock()
        whenever(extension.getConnectedPort(defaultPort, session)).thenReturn(mock())

        assertFalse(controller.portConnected(null))
        assertFalse(controller.portConnected(mock()))
        assertFalse(controller.portConnected(session))

        WebExtensionController.installedExtensions[extensionId] = extension

        assertTrue(controller.portConnected(session))
        assertFalse(controller.portConnected(session, "invalid"))
    }

    @Test
    fun `disconnect port`() {
        val extension: WebExtension = mock()
        val controller = WebExtensionController(extensionId, extensionUrl, defaultPort)

        controller.disconnectPort(null)
        verify(extension, never()).disconnectPort(eq(defaultPort), any())

        val session: EngineSession = mock()
        controller.disconnectPort(session)
        verify(extension, never()).disconnectPort(eq(defaultPort), eq(session))

        WebExtensionController.installedExtensions[extensionId] = extension
        controller.disconnectPort(session)
        verify(extension, times(1)).disconnectPort(eq(defaultPort), eq(session))
    }
}
