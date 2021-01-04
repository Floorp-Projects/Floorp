package mozilla.components.browser.session.storage/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.support.ktx.util.writeString
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * The test cases in this class restore actual browsing sessions taken from devices. If a test case
 * breaks then this is a strong indication that the restore code changed in a non backwards compatible
 * way and that users may lose data.
 *
 * The tests here do also restore the "engine" state using GeckoView Nightly. Breaking changes in
 * GeckoView may also cause session restore to break in a non backwards compatible way. Such breakages
 * need to be reported to the GeckoView team to be fixed before reaching release branches.
 */
@Suppress("TestFunctionName")
@RunWith(AndroidJUnit4::class)
class RestoringBrowsingSessionsTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    /**
     * App: Sample Browser
     * Version: 2020-12-16
     * GeckoView: 84.0 Beta (3147bf02ca678985f814ea2b091a194a9ac22f71)
     */
    @Test
    fun SampleBrowser_2020_12_16() = runBlocking(Dispatchers.Main) {
        val json = """
            {"version":2,"selectedTabId":"55a0b8ce-d2ef-4a19-b921-5d28fbe6906f","sessionStateTuples":[{"session":{"url":"https://www.mozilla.org/en-US/about/manifesto/","uuid":"497e5d15-c03d-434e-8b06-9186886c42ed","parentUuid":"","title":"The Mozilla Manifesto","contextId":null,"readerMode":false,"lastAccess":0},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1977,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"presState\":[{\"scroll\":\"0,19140\",\"scrollOriginDowngrade\":false,\"stateKey\":\"0>3>fp>1>0>\"}],\"persist\":true,\"cacheKey\":0,\"ID\":0,\"url\":\"https:\\/\\/www.mozilla.org\\/en-US\\/\",\"title\":\"Internet for people, not profit — Mozilla\",\"loadReplace\":true,\"docIdentifier\":2147483649,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7OTM1ZGYxMjAtMDJlNS00MzcyLTg5MTQtZmFhYmM5OTdmZjBmfSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7OTI1MzlkYjMtNGYwZS00YTliLWE0NzUtMjllMWMwZTdiYmUyfSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7OTM1ZGYxMjAtMDJlNS00MzcyLTg5MTQtZmFhYmM5OTdmZjBmfSJ9fQ==\",\"resultPrincipalURI\":\"https:\\/\\/www.mozilla.org\\/en-US\\/\",\"hasUserInteraction\":true,\"originalURI\":\"http:\\/\\/mozilla.org\\/\",\"docshellUUID\":\"{94b7df7c-ce82-4489-9cbf-b317dde845db}\"},{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYBAAAAHmh0dHBzOi8vd3d3Lm1vemlsbGEub3JnL2VuLVVTLwAAAAEBAQAAAB5odHRwczovL3d3dy5tb3ppbGxhLm9yZy9lbi1VUy8BAA==\",\"persist\":true,\"cacheKey\":0,\"ID\":2,\"csp\":\"CdntGuXUQAS\\/4CfOuSPZrAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC\\/\\/\\/\\/\\/wAAAbsBAAAAHmh0dHBzOi8vd3d3Lm1vemlsbGEub3JnL2VuLVVTLwAAAAAAAAAFAAAACAAAAA8AAAAA\\/\\/\\/\\/\\/wAAAAD\\/\\/\\/\\/\\/AAAACAAAAA8AAAAXAAAABwAAABcAAAAHAAAAFwAAAAcAAAAeAAAAAAAAAAD\\/\\/\\/\\/\\/AAAAAP\\/\\/\\/\\/8AAAAA\\/\\/\\/\\/\\/wAAAAD\\/\\/\\/\\/\\/AQAAAAAAAAAAACx7IjEiOnsiMCI6Imh0dHBzOi8vd3d3Lm1vemlsbGEub3JnL2VuLVVTLyJ9fQAAAAEAAAg3AGQAZQBmAGEAdQBsAHQALQBzAHIAYwAgACcAcwBlAGwAZgAnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbgBlAHQAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAGMAbwBtADsAIABzAGMAcgBpAHAAdAAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AIAAnAHUAbgBzAGEAZgBlAC0AaQBuAGwAaQBuAGUAJwAgACcAdQBuAHMAYQBmAGUALQBlAHYAYQBsACcAIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQB0AGEAZwBtAGEAbgBhAGcAZQByAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQAtAGEAbgBhAGwAeQB0AGkAYwBzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdABhAGcAbQBhAG4AYQBnAGUAcgAuAGcAbwBvAGcAbABlAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgB5AG8AdQB0AHUAYgBlAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AcwAuAHkAdABpAG0AZwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGMAZABuAC0AMwAuAGMAbwBuAHYAZQByAHQAZQB4AHAAZQByAGkAbQBlAG4AdABzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AYQBwAHAALgBjAG8AbgB2AGUAcgB0AC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AZABhAHQAYQAuAHQAcgBhAGMAawAuAGMAbwBuAHYAZQByAHQAZQB4AHAAZQByAGkAbQBlAG4AdABzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AMQAwADAAMwAzADUAMAAuAHQAcgBhAGMAawAuAGMAbwBuAHYAZQByAHQAZQB4AHAAZQByAGkAbQBlAG4AdABzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AMQAwADAAMwAzADQAMwAuAHQAcgBhAGMAawAuAGMAbwBuAHYAZQByAHQAZQB4AHAAZQByAGkAbQBlAG4AdABzAC4AYwBvAG0AOwAgAHMAdAB5AGwAZQAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AIAAnAHUAbgBzAGEAZgBlAC0AaQBuAGwAaQBuAGUAJwAgAGgAdAB0AHAAcwA6AC8ALwBhAHAAcAAuAGMAbwBuAHYAZQByAHQALgBjAG8AbQA7ACAAYwBoAGkAbABkAC0AcwByAGMAIAAnAHMAZQBsAGYAJwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG4AZQB0ACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAHQAYQBnAG0AYQBuAGEAZwBlAHIALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC0AYQBuAGEAbAB5AHQAaQBjAHMALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAHkAbwB1AHQAdQBiAGUALQBuAG8AYwBvAG8AawBpAGUALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB0AHIAYQBjAGsAZQByAHQAZQBzAHQALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAHMAdQByAHYAZQB5AGcAaQB6AG0AbwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGEAYwBjAG8AdQBuAHQAcwAuAGYAaQByAGUAZgBvAHgALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGMAYwBvAHUAbgB0AHMALgBmAGkAcgBlAGYAbwB4AC4AYwBvAG0ALgBjAG4AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgB5AG8AdQB0AHUAYgBlAC4AYwBvAG0AOwAgAGkAbQBnAC0AcwByAGMAIAAnAHMAZQBsAGYAJwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG4AZQB0ACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBjAG8AbQAgAGQAYQB0AGEAOgAgAGgAdAB0AHAAcwA6AC8ALwBtAG8AegBpAGwAbABhAC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AZgBpAHIAZQBmAG8AeAAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUAdABhAGcAbQBhAG4AYQBnAGUAcgAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALQBhAG4AYQBsAHkAdABpAGMAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGEAZABzAGUAcgB2AGkAYwBlAC4AZwBvAG8AZwBsAGUALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGQAcwBlAHIAdgBpAGMAZQAuAGcAbwBvAGcAbABlAC4AZABlACAAaAB0AHQAcABzADoALwAvAGEAZABzAGUAcgB2AGkAYwBlAC4AZwBvAG8AZwBsAGUALgBkAGsAIABoAHQAdABwAHMAOgAvAC8AYwByAGUAYQB0AGkAdgBlAGMAbwBtAG0AbwBuAHMALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwBjAGQAbgAtADMALgBjAG8AbgB2AGUAcgB0AGUAeABwAGUAcgBpAG0AZQBuAHQAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGwAbwBnAHMALgBjAG8AbgB2AGUAcgB0AGUAeABwAGUAcgBpAG0AZQBuAHQAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGEAZAAuAGQAbwB1AGIAbABlAGMAbABpAGMAawAuAG4AZQB0ADsAIABjAG8AbgBuAGUAYwB0AC0AcwByAGMAIAAnAHMAZQBsAGYAJwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG4AZQB0ACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAHQAYQBnAG0AYQBuAGEAZwBlAHIALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC0AYQBuAGEAbAB5AHQAaQBjAHMALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBsAG8AZwBzAC4AYwBvAG4AdgBlAHIAdABlAHgAcABlAHIAaQBtAGUAbgB0AHMALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwAxADAAMAAzADMANQAwAC4AbQBlAHQAcgBpAGMAcwAuAGMAbwBuAHYAZQByAHQAZQB4AHAAZQByAGkAbQBlAG4AdABzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AMQAwADAAMwAzADQAMwAuAG0AZQB0AHIAaQBjAHMALgBjAG8AbgB2AGUAcgB0AGUAeABwAGUAcgBpAG0AZQBuAHQAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHMAZQBuAHQAcgB5AC4AcAByAG8AZAAuAG0AbwB6AGEAdwBzAC4AbgBlAHQAIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtAC8AIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtAC4AYwBuAC8AOwAgAGYAcgBhAG0AZQAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQB0AGEAZwBtAGEAbgBhAGcAZQByAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQAtAGEAbgBhAGwAeQB0AGkAYwBzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgB5AG8AdQB0AHUAYgBlAC0AbgBvAGMAbwBvAGsAaQBlAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdAByAGEAYwBrAGUAcgB0AGUAcwB0AC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBzAHUAcgB2AGUAeQBnAGkAegBtAG8ALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGMAYwBvAHUAbgB0AHMALgBmAGkAcgBlAGYAbwB4AC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtAC4AYwBuACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AeQBvAHUAdAB1AGIAZQAuAGMAbwBtAAA=\",\"url\":\"https:\\/\\/www.mozilla.org\\/en-US\\/about\\/manifesto\\/\",\"title\":\"The Mozilla Manifesto\",\"loadReplace\":false,\"docIdentifier\":2147483651,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5tb3ppbGxhLm9yZy9lbi1VUy8ifX0=\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5tb3ppbGxhLm9yZy9lbi1VUy8ifX0=\",\"principalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy5tb3ppbGxhLm9yZy9lbi1VUy8ifX0=\",\"resultPrincipalURI\":\"https:\\/\\/www.mozilla.org\\/en-US\\/about\\/manifesto\\/\",\"hasUserInteraction\":false,\"originalURI\":\"https:\\/\\/www.mozilla.org\\/en-US\\/about\\/manifesto\\/\",\"docshellUUID\":\"{94b7df7c-ce82-4489-9cbf-b317dde845db}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":2,\"userContextId\":0}}"}},{"session":{"url":"https://www.mozilla.org/en-US/firefox/new/?redirect_source=firefox-com","uuid":"55a0b8ce-d2ef-4a19-b921-5d28fbe6906f","parentUuid":"","title":"Download Firefox Browser — Fast, Private & Free — from Mozilla","contextId":null,"readerMode":false,"lastAccess":1608115274898},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1977,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"persist\":true,\"cacheKey\":0,\"ID\":4,\"url\":\"https:\\/\\/www.mozilla.org\\/en-US\\/firefox\\/new\\/?redirect_source=firefox-com\",\"title\":\"Download Firefox Browser — Fast, Private & Free — from Mozilla\",\"loadReplace\":true,\"docIdentifier\":2147483653,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7MjAzN2M0MTktOTQwMC00NTdhLTgxNGEtNjNlODI0MWNmY2Y0fSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7ODE4NzVjYzItZjZkMC00MDg5LWE4ZTAtMzlkY2I2MmMyOGU4fSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7MjAzN2M0MTktOTQwMC00NTdhLTgxNGEtNjNlODI0MWNmY2Y0fSJ9fQ==\",\"resultPrincipalURI\":\"https:\\/\\/www.mozilla.org\\/en-US\\/firefox\\/new\\/?redirect_source=firefox-com\",\"hasUserInteraction\":true,\"originalURI\":\"http:\\/\\/firefox.com\\/\",\"docshellUUID\":\"{75f3a875-5651-4881-82f1-68f333aa2062}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":1,\"userContextId\":0}}"}}]}
        """.trimIndent()

        val engine = GeckoEngine(context)

        assertTrue(
            getFileForEngine(context, engine).writeString { json }
        )

        val storage = SessionStorage(context, engine)
        val browsingSession = storage.restore()

        assertNotNull(browsingSession)

        assertEquals(2, browsingSession!!.tabs.size)
        assertEquals("55a0b8ce-d2ef-4a19-b921-5d28fbe6906f", browsingSession.selectedTabId)

        val tab1 = browsingSession.tabs[0]
        val tab2 = browsingSession.tabs[1]

        assertEquals("497e5d15-c03d-434e-8b06-9186886c42ed", tab1.id)
        assertEquals("The Mozilla Manifesto", tab1.title)
        assertNull(tab1.contextId)
        assertNull(tab1.parentId)
        assertFalse(tab1.readerState.active)
        assertNull(tab1.readerState.activeUrl)
        assertEquals(0, tab1.lastAccess)
        assertNotNull(tab1.state)

        assertEquals("55a0b8ce-d2ef-4a19-b921-5d28fbe6906f", tab2.id)
        assertEquals("Download Firefox Browser — Fast, Private & Free — from Mozilla", tab2.title)
        assertNull(tab1.contextId)
        assertNull(tab1.parentId)
        assertFalse(tab2.readerState.active)
        assertNull(tab2.readerState.activeUrl)
        assertEquals(1608115274898, tab2.lastAccess)
        assertNotNull(tab2.state)
    }
}
