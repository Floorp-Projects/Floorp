/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry.ads

import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_EXTENSION_ID
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_EXTENSION_RESOURCE_URL
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_MESSAGE_DOCUMENT_URLS_KEY
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_MESSAGE_SESSION_URL_KEY

@RunWith(FenixRobolectricTestRunner::class)
class AdsTelemetryTest {

    private val metrics: MetricController = mockk(relaxed = true)
    private lateinit var ads: AdsTelemetry
    private lateinit var adsMessageHandler: AdsTelemetry.AdsTelemetryContentMessageHandler

    @Before
    fun setUp() {
        ads = spyk(AdsTelemetry(metrics))
        adsMessageHandler = ads.AdsTelemetryContentMessageHandler()
    }

    @Test
    fun `don't track with null session url`() {
        ads.trackAdClickedMetric(null, listOf())

        verify(exactly = 0) { ads.getProviderForUrl(any()) }
    }

    @Test
    fun `don't track when no ads are in the redirect path`() {
        val sessionUrl = "https://www.google.com/search?q=aaa"

        ads.trackAdClickedMetric(sessionUrl, listOf("https://www.aaa.com"))

        verify(exactly = 0) { metrics.track(any()) }
    }

    @Test
    fun `track when ads are in the redirect path`() {
        val metricEvent = slot<Event.SearchAdClicked>()
        val sessionUrl = "https://www.google.com/search?q=aaa"

        ads.trackAdClickedMetric(
            sessionUrl,
            listOf("https://www.google.com/aclk", "https://www.aaa.com")
        )

        verify { metrics.track(capture(metricEvent)) }
        assertEquals(ads.providerList[0].name, metricEvent.captured.label)
    }

    @Test
    fun install() {
        val engine = mockk<Engine>(relaxed = true)
        val store = mockk<BrowserStore>(relaxed = true)

        ads.install(engine, store)

        verify {
            engine.installWebExtension(
                id = ADS_EXTENSION_ID,
                url = ADS_EXTENSION_RESOURCE_URL,
                allowContentMessaging = true,
                onSuccess = any(),
                onError = any()
            )
        }
    }

    @Test
    fun `message handler processes the document urls and reports an ad`() {
        val metricEvent = slot<Event.SearchWithAds>()
        val first = "https://www.google.com/aclk"
        val second = "https://www.google.com/aaa"
        val array = JSONArray()
        array.put(first)
        array.put(second)
        val message = JSONObject()
        message.put(ADS_MESSAGE_DOCUMENT_URLS_KEY, array)
        message.put(ADS_MESSAGE_SESSION_URL_KEY, "https://www.google.com/search?q=aaa")

        assertEquals("", adsMessageHandler.onMessage(message, mockk()))

        verify { metrics.track(capture(metricEvent)) }
        assertEquals(ads.providerList[0].name, metricEvent.captured.label)
    }

    @Test
    fun `message handler processes the document urls and doesn't find ads`() {
        val first = "https://www.google.com/aaaaaa"
        val second = "https://www.google.com/aaa"
        val array = JSONArray()
        array.put(first)
        array.put(second)
        val message = JSONObject()
        message.put(ADS_MESSAGE_DOCUMENT_URLS_KEY, array)
        message.put(ADS_MESSAGE_SESSION_URL_KEY, "https://www.google.com/search?q=aaa")

        assertEquals("", adsMessageHandler.onMessage(message, mockk()))

        verify(exactly = 0) { metrics.track(any()) }
    }

    @Test(expected = IllegalStateException::class)
    fun `message handler finds no json object`() {
        val message = "message"

        adsMessageHandler.onMessage(message, mockk())
    }
}
