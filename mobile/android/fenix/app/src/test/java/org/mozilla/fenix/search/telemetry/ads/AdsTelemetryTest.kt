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
import org.mozilla.fenix.search.telemetry.ExtensionInfo
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_EXTENSION_ID
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_EXTENSION_RESOURCE_URL
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_MESSAGE_DOCUMENT_URLS_KEY
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_MESSAGE_ID
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry.Companion.ADS_MESSAGE_SESSION_URL_KEY

@RunWith(FenixRobolectricTestRunner::class)
class AdsTelemetryTest {

    private val metrics: MetricController = mockk(relaxed = true)
    private lateinit var ads: AdsTelemetry

    @Before
    fun setUp() {
        ads = spyk(AdsTelemetry(metrics))
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
        val extensionInfo = slot<ExtensionInfo>()

        ads.install(engine, store)

        verify { ads.installWebExtension(engine, store, capture(extensionInfo)) }
        assertEquals(ADS_EXTENSION_ID, extensionInfo.captured.id)
        assertEquals(ADS_EXTENSION_RESOURCE_URL, extensionInfo.captured.resourceUrl)
        assertEquals(ADS_MESSAGE_ID, extensionInfo.captured.messageId)
    }

    @Test
    fun `process the document urls and reports an ad`() {
        val metricEvent = slot<Event.SearchWithAds>()
        val first = "https://www.google.com/aclk"
        val second = "https://www.google.com/aaa"
        val array = JSONArray()
        array.put(first)
        array.put(second)
        val message = JSONObject()
        message.put(ADS_MESSAGE_DOCUMENT_URLS_KEY, array)
        message.put(ADS_MESSAGE_SESSION_URL_KEY, "https://www.google.com/search?q=aaa")

        ads.processMessage(message)

        verify { metrics.track(capture(metricEvent)) }
        assertEquals(ads.providerList[0].name, metricEvent.captured.label)
    }

    @Test
    fun `process the document urls and don't find ads`() {
        val first = "https://www.google.com/aaaaaa"
        val second = "https://www.google.com/aaa"
        val array = JSONArray()
        array.put(first)
        array.put(second)
        val message = JSONObject()
        message.put(ADS_MESSAGE_DOCUMENT_URLS_KEY, array)
        message.put(ADS_MESSAGE_SESSION_URL_KEY, "https://www.google.com/search?q=aaa")

        ads.processMessage(message)

        verify(exactly = 0) { metrics.track(any()) }
    }
}
