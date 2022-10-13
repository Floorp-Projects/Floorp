/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webcompat.reporter

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.webextensions.WebExtensionController
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebCompatReporterFeatureTest {

    @Before
    fun setup() {
        WebExtensionController.installedExtensions.clear()
    }

    @Test
    fun `installs the webextension`() {
        val engine: Engine = mock()
        val controller: WebExtensionController = mock()
        installFeatureForTest(engine, controller)
    }

    @Test
    fun `install registers the background message handler`() {
        val engine: Engine = mock()
        val controller: WebExtensionController = mock()
        installFeatureForTest(engine, controller)

        verify(controller).registerBackgroundMessageHandler(any(), any())
    }

    @Test
    fun `backgroundMessageHandler sends the default productName if unset`() {
        val engine: Engine = mock()
        val controller: WebExtensionController = mock()
        installFeatureForTest(engine, controller)

        val messageHandler = argumentCaptor<MessageHandler>()
        verify(controller).registerBackgroundMessageHandler(messageHandler.capture(), any())

        val port: Port = mock()
        val message = argumentCaptor<JSONObject>()
        messageHandler.value.onPortConnected(port)
        verify(port).postMessage(message.capture())

        val productNameMessage = JSONObject().put("productName", "android-components")
        verify(port, times(1)).postMessage(message.capture())

        assertEquals(productNameMessage.toString(), message.value.toString())
    }

    @Test
    fun `backgroundMessageHandler sends the correct productName if set`() {
        val engine: Engine = mock()
        val controller: WebExtensionController = mock()
        installFeatureForTest(engine, controller, "test")

        val messageHandler = argumentCaptor<MessageHandler>()
        verify(controller).registerBackgroundMessageHandler(messageHandler.capture(), any())

        val port: Port = mock()
        val message = argumentCaptor<JSONObject>()
        messageHandler.value.onPortConnected(port)
        verify(port).postMessage(message.capture())

        val productNameMessage = JSONObject().put("productName", "test")
        verify(port, times(1)).postMessage(message.capture())

        assertEquals(productNameMessage.toString(), message.value.toString())
    }

    private fun installFeatureForTest(
        engine: Engine,
        controller: WebExtensionController,
        productName: String? = null,
    ): WebCompatReporterFeature {
        val reporterFeature = spy(WebCompatReporterFeature)
        reporterFeature.extensionController = controller

        if (productName == null) {
            reporterFeature.install(engine)
        } else {
            reporterFeature.install(engine, productName)
        }

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((Throwable) -> Unit)>()
        verify(controller, times(1)).install(
            eq(engine),
            onSuccess.capture(),
            onError.capture(),
        )

        onSuccess.value.invoke(mock())
        return reporterFeature
    }
}
