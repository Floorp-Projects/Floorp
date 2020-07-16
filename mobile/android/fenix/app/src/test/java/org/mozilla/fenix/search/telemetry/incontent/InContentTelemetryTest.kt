package org.mozilla.fenix.search.telemetry.incontent

import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.search.telemetry.ExtensionInfo
import org.mozilla.fenix.search.telemetry.incontent.InContentTelemetry.Companion.COOKIES_EXTENSION_ID
import org.mozilla.fenix.search.telemetry.incontent.InContentTelemetry.Companion.COOKIES_EXTENSION_RESOURCE_URL
import org.mozilla.fenix.search.telemetry.incontent.InContentTelemetry.Companion.COOKIES_MESSAGE_ID
import org.mozilla.fenix.search.telemetry.incontent.InContentTelemetry.Companion.COOKIES_MESSAGE_LIST_KEY
import org.mozilla.fenix.search.telemetry.incontent.InContentTelemetry.Companion.COOKIES_MESSAGE_SESSION_URL_KEY

@RunWith(FenixRobolectricTestRunner::class)
class InContentTelemetryTest {

    private val metrics: MetricController = mockk(relaxed = true)
    private lateinit var telemetry: InContentTelemetry

    @Before
    fun setUp() {
        telemetry = spyk(InContentTelemetry(metrics))
    }

    @Test
    fun install() {
        val engine = mockk<Engine>(relaxed = true)
        val store = mockk<BrowserStore>(relaxed = true)
        val extensionInfo = slot<ExtensionInfo>()

        telemetry.install(engine, store)

        verify { telemetry.installWebExtension(engine, store, capture(extensionInfo)) }
        Assert.assertEquals(COOKIES_EXTENSION_ID, extensionInfo.captured.id)
        Assert.assertEquals(COOKIES_EXTENSION_RESOURCE_URL, extensionInfo.captured.resourceUrl)
        Assert.assertEquals(COOKIES_MESSAGE_ID, extensionInfo.captured.messageId)
    }

    @Test
    fun processMessage() {
        val first = JSONObject()
        val second = JSONObject()
        val array = JSONArray()
        array.put(first)
        array.put(second)
        val message = JSONObject()
        val url = "https://www.google.com/search?q=aaa"
        message.put(COOKIES_MESSAGE_LIST_KEY, array)
        message.put(COOKIES_MESSAGE_SESSION_URL_KEY, url)

        telemetry.processMessage(message)

        verify { telemetry.trackPartnerUrlTypeMetric(url, listOf(first, second)) }
    }

    @Test
    fun `track google sap metric`() {
        val url = "https://www.google.com/search?q=aaa&client=firefox-b-m"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("google.in-content.sap.firefox-b-m")) }
    }

    @Test
    fun `track duckduckgo sap metric`() {
        val url = "https://duckduckgo.com/?q=aaa&t=fpas"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("duckduckgo.in-content.sap.fpas")) }
    }

    @Test
    fun `track baidu sap metric`() {
        val url = "https://www.baidu.com/from=844b/s?wd=aaa&tn=34046034_firefox"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("baidu.in-content.sap.34046034_firefox")) }
    }

    @Test
    fun `track bing sap metric`() {
        val url = "https://www.bing.com/search?q=aaa&pc=MOZMBA"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("bing.in-content.sap.mozmba")) }
    }

    @Test
    fun `track google sap-follow-on metric`() {
        val url = "https://www.google.com/search?q=aaa&client=firefox-b-m&oq=random"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("google.in-content.sap-follow-on.firefox-b-m")) }
    }

    @Test
    fun `track baidu sap-follow-on metric`() {
        val url = "https://www.baidu.com/from=844b/s?wd=aaa&tn=34046034_firefox&oq=random"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("baidu.in-content.sap-follow-on.34046034_firefox")) }
    }

    @Test
    fun `track bing sap-follow-on metric by cookies`() {
        val url = "https://www.bing.com/search?q=aaa&pc=MOZMBA&form=QBRERANDOM"

        telemetry.trackPartnerUrlTypeMetric(url, createCookieList())

        verify { metrics.track(Event.SearchInContent("bing.in-content.sap-follow-on.mozmba")) }
    }

    @Test
    fun `track google organic metric`() {
        val url = "https://www.google.com/search?q=aaa"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("google.in-content.organic.none")) }
    }

    @Test
    fun `track duckduckgo organic metric`() {
        val url = "https://duckduckgo.com/?q=aaa"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("duckduckgo.in-content.organic.none")) }
    }

    @Test
    fun `track bing organic metric`() {
        val url = "https://www.bing.com/search?q=aaa"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("bing.in-content.organic.none")) }
    }

    @Test
    fun `track baidu organic metric`() {
        val url = "https://www.baidu.com/from=844b/s?wd=aaa"

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        verify { metrics.track(Event.SearchInContent("baidu.in-content.organic.none")) }
    }

    private fun createCookieList(): List<JSONObject> {
        val first = JSONObject()
        first.put("name", "SRCHS")
        first.put("value", "PC=MOZMBA")
        val second = JSONObject()
        second.put("name", "RANDOM")
        second.put("value", "RANDOM")
        return listOf(first, second)
    }
}
