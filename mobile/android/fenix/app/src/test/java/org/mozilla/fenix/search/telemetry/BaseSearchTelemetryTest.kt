package org.mozilla.fenix.search.telemetry

import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test

class BaseSearchTelemetryTest {

    private lateinit var baseTelemetry: BaseSearchTelemetry
    private lateinit var handler: BaseSearchTelemetry.SearchTelemetryMessageHandler

    @org.junit.Before
    fun setUp() {
        baseTelemetry = spyk(object : BaseSearchTelemetry() {

            override fun install(engine: Engine, store: BrowserStore) {
                // mock, do nothing
            }

            override fun processMessage(message: JSONObject) {
                // mock, do nothing
            }
        })
        handler = baseTelemetry.SearchTelemetryMessageHandler()
    }

    @Test
    fun install() {
        val engine = mockk<Engine>(relaxed = true)
        val store = mockk<BrowserStore>(relaxed = true)
        val id = "id"
        val resourceUrl = "resourceUrl"
        val messageId = "messageId"
        val extensionInfo = ExtensionInfo(id, resourceUrl, messageId)

        baseTelemetry.installWebExtension(engine, store, extensionInfo)

        verify {
            engine.installWebExtension(
                id = id,
                url = resourceUrl,
                allowContentMessaging = true,
                onSuccess = any(),
                onError = any()
            )
        }
    }

    @Test
    fun `get provider for google url`() {
        val url = "https://www.google.com/search?q=computers"

        assertEquals("google", baseTelemetry.getProviderForUrl(url)?.name)
    }

    @Test
    fun `message handler finds a valid json object`() {
        val message = JSONObject()

        handler.onMessage(message, mockk())

        verify { baseTelemetry.processMessage(message) }
    }

    @Test(expected = IllegalStateException::class)
    fun `message handler finds no json object`() {
        val message = "message"

        handler.onMessage(message, mockk())
    }
}
