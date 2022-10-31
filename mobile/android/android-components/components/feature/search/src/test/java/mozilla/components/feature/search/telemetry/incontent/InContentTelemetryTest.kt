/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry.incontent

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.search.telemetry.ExtensionInfo
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry.Companion.SEARCH_EXTENSION_ID
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry.Companion.SEARCH_EXTENSION_RESOURCE_URL
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry.Companion.SEARCH_MESSAGE_ID
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry.Companion.SEARCH_MESSAGE_LIST_KEY
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry.Companion.SEARCH_MESSAGE_SESSION_URL_KEY
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class InContentTelemetryTest {
    private lateinit var telemetry: InContentTelemetry

    @Before
    fun setup() {
        telemetry = spy(InContentTelemetry())
    }

    @Test
    fun `WHEN installWebExtension is called THEN install a properly configured extension`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val extensionCaptor = argumentCaptor<ExtensionInfo>()

        telemetry.install(engine, store)

        verify(telemetry).installWebExtension(eq(engine), eq(store), extensionCaptor.capture())
        assertEquals(SEARCH_EXTENSION_ID, extensionCaptor.value.id)
        assertEquals(SEARCH_EXTENSION_RESOURCE_URL, extensionCaptor.value.resourceUrl)
        assertEquals(SEARCH_MESSAGE_ID, extensionCaptor.value.messageId)
    }

    @Test
    fun `GIVEN a message from the extension WHEN processMessage is called THEN track the search`() {
        val first = JSONObject()
        val second = JSONObject()
        val array = JSONArray()
        array.put(first)
        array.put(second)
        val message = JSONObject()
        val url = "https://www.google.com/search?q=aaa"
        message.put(SEARCH_MESSAGE_LIST_KEY, array)
        message.put(SEARCH_MESSAGE_SESSION_URL_KEY, url)

        telemetry.processMessage(message)

        verify(telemetry).trackPartnerUrlTypeMetric(url, listOf(first, second))
    }

    @Test
    fun `GIVEN a Google search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.google.com/search?q=aaa&client=firefox-b-m"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("google.in-content.sap.firefox-b-m", facts[0].value)
    }

    @Test
    fun `GIVEN a DuckDuckGo search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://duckduckgo.com/?q=aaa&t=fpas"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("duckduckgo.in-content.sap.fpas", facts[0].value)
    }

    @Test
    fun `GIVEN a Baidu search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://m.baidu.com/s?from=1000969a&word=aaa"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("baidu.in-content.sap._1000969a", facts[0].value)
    }

    @Test
    fun `GIVEN an invalid Bing search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.bing.com/search?q=aaa&pc=MOZMBA"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("bing.in-content.sap.other", facts[0].value)
    }

    @Test
    fun `GIVEN a Google sap-follow-on WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.google.com/search?q=aaa&client=firefox-b-m&oq=random"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("google.in-content.sap-follow-on.firefox-b-m", facts[0].value)
    }

    @Test
    fun `GIVEN an invalid Google sap-follow-on WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.google.com/search?q=aaa&client=firefox-b-mTesting&oq=random"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("google.in-content.sap-follow-on.other", facts[0].value)
    }

    @Test
    fun `GIVEN a Google sap-follow-on from topSite WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.google.com/search?q=aaa&client=firefox-b-m&channel=ts&oq=random"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("google.in-content.sap-follow-on.firefox-b-m.ts", facts[0].value)
    }

    @Test
    fun `GIVEN an invalid Google channel from topSite WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.google.com/search?q=aaa&client=firefox-b-m&channel=tsTest&oq=random"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("google.in-content.sap-follow-on.firefox-b-m", facts[0].value)
    }

    @Test
    fun `GIVEN a Baidu sap-follow-on WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://m.baidu.com/s?from=1000969a&word=aaa&oq=random"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("baidu.in-content.sap-follow-on._1000969a", facts[0].value)
    }

    @Test
    fun `GIVEN a Bing sap-follow-on with cookies WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.bing.com/search?q=aaa&pc=MOZMBA&form=QBRERANDOM"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, createCookieList())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("bing.in-content.sap-follow-on.mozmba", facts[0].value)
    }

    @Test
    fun `GIVEN a Google organic search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.google.com/search?q=aaa"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("google.in-content.organic.none", facts[0].value)
    }

    @Test
    fun `GIVEN a DuckDuckGo organic search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://duckduckgo.com/?q=aaa"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("duckduckgo.in-content.organic.none", facts[0].value)
    }

    @Test
    fun `GIVEN a Bing organic search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://www.bing.com/search?q=aaa"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("bing.in-content.organic.none", facts[0].value)
    }

    @Test
    fun `GIVEN a Baidu organic search WHEN trackPartnerUrlTypeMetric is called THEN emit an appropriate IN_CONTENT_SEARCH fact`() {
        val url = "https://m.baidu.com/s?word=aaa"
        val facts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            },
        )

        telemetry.trackPartnerUrlTypeMetric(url, listOf())

        assertEquals(1, facts.size)
        assertEquals(Component.FEATURE_SEARCH, facts[0].component)
        assertEquals(Action.INTERACTION, facts[0].action)
        assertEquals(InContentTelemetry.IN_CONTENT_SEARCH, facts[0].item)
        assertEquals("baidu.in-content.organic.none", facts[0].value)
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
