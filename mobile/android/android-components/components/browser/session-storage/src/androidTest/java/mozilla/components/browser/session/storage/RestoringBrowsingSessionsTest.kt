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
            getFileForEngine(context, engine).writeString { json },
        )

        val storage = SessionStorage(context, engine)
        val state = storage.restore()

        assertNotNull(state)

        assertEquals(2, state!!.tabs.size)
        assertEquals("55a0b8ce-d2ef-4a19-b921-5d28fbe6906f", state.selectedTabId)

        val tab1 = state.tabs[0]
        val tab2 = state.tabs[1]

        assertEquals("497e5d15-c03d-434e-8b06-9186886c42ed", tab1.state.id)
        assertEquals("The Mozilla Manifesto", tab1.state.title)
        assertNull(tab1.state.contextId)
        assertNull(tab1.state.parentId)
        assertFalse(tab1.state.readerState.active)
        assertNull(tab1.state.readerState.activeUrl)
        assertEquals(0, tab1.state.lastAccess)
        assertNotNull(tab1.engineSessionState)

        assertEquals("55a0b8ce-d2ef-4a19-b921-5d28fbe6906f", tab2.state.id)
        assertEquals("Download Firefox Browser — Fast, Private & Free — from Mozilla", tab2.state.title)
        assertNull(tab1.state.contextId)
        assertNull(tab1.state.parentId)
        assertFalse(tab2.state.readerState.active)
        assertNull(tab2.state.readerState.activeUrl)
        assertEquals(1608115274898, tab2.state.lastAccess)
        assertNotNull(tab2.engineSessionState)
    }

    /**
     * App: Firefox for Android (Fenix)
     * Version: 80.1.3
     */
    @Test
    fun Fenix_80_1_3() {
        runBlocking(Dispatchers.Main) {
            val json = """
                {"version":1,"selectedSessionIndex":2,"sessionStateTuples":[{"session":{"url":"https:\/\/en.m.wikipedia.org\/wiki\/Fox","uuid":"f40b3385-7e75-47b3-9c54-15a02afa27a4","parentUuid":"","title":"Fox - Wikipedia","readerMode":false},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1823,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"presState\":[{\"scroll\":\"0,15488\",\"scrollOriginDowngrade\":false,\"stateKey\":\"0>3>fp>0>4>\"}],\"persist\":true,\"cacheKey\":0,\"ID\":0,\"url\":\"https:\\\/\\\/www.wikipedia.org\\\/\",\"title\":\"Wikipedia\",\"loadReplace\":false,\"docIdentifier\":1,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7ODdkZDY0OTItYzMxMi00OGMzLWJmMDMtOGZmOGI5ZTI2NjlifSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7OTNhMGUwOWYtNTJmYS00MmZmLWFiNTEtMGEyMjQ0ZGIwMzdmfSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7ODdkZDY0OTItYzMxMi00OGMzLWJmMDMtOGZmOGI5ZTI2NjlifSJ9fQ==\",\"resultPrincipalURI\":\"https:\\\/\\\/www.wikipedia.org\\\/\",\"hasUserInteraction\":true,\"originalURI\":\"https:\\\/\\\/www.wikipedia.org\\\/\",\"docshellUUID\":\"{06a76895-6632-41cb-8dbe-0ce6d9d63418}\"},{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYBAAAAGmh0dHBzOi8vd3d3Lndpa2lwZWRpYS5vcmcvAAAAAQEBAAAAGmh0dHBzOi8vd3d3Lndpa2lwZWRpYS5vcmcvAQA=\",\"persist\":true,\"cacheKey\":0,\"ID\":1,\"csp\":\"CdntGuXUQAS\\\/4CfOuSPZrAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC\\\/\\\/\\\/\\\/\\\/wAAAbsBAAAAGmh0dHBzOi8vd3d3Lndpa2lwZWRpYS5vcmcvAAAAAAAAAAUAAAAIAAAAEQAAAAD\\\/\\\/\\\/\\\/\\\/AAAAAP\\\/\\\/\\\/\\\/8AAAAIAAAAEQAAABkAAAABAAAAGQAAAAEAAAAZAAAAAQAAABoAAAAAAAAAAP\\\/\\\/\\\/\\\/8AAAAA\\\/\\\/\\\/\\\/\\\/wAAAAD\\\/\\\/\\\/\\\/\\\/AAAAAP\\\/\\\/\\\/\\\/8BAAAAAAAAAAAAKHsiMSI6eyIwIjoiaHR0cHM6Ly93d3cud2lraXBlZGlhLm9yZy8ifX0AAAAA\",\"url\":\"https:\\\/\\\/en.m.wikipedia.org\\\/wiki\\\/Fox\",\"title\":\"Fox - Wikipedia\",\"loadReplace\":true,\"docIdentifier\":2,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7ZGY5YmEyNmItN2MyNy00YmFlLWI1ZGItYzYwZmVjYWRjMWQwfSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy53aWtpcGVkaWEub3JnLyJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7ZGY5YmEyNmItN2MyNy00YmFlLWI1ZGItYzYwZmVjYWRjMWQwfSJ9fQ==\",\"resultPrincipalURI\":\"https:\\\/\\\/en.m.wikipedia.org\\\/wiki\\\/Fox\",\"hasUserInteraction\":false,\"originalURI\":\"https:\\\/\\\/www.wikipedia.org\\\/search-redirect.php?family=wikipedia&language=en&search=fox&language=en&go=Go\",\"docshellUUID\":\"{06a76895-6632-41cb-8dbe-0ce6d9d63418}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":2,\"userContextId\":0}}"}},{"session":{"url":"https:\/\/www.theverge.com\/2021\/1\/4\/22212347\/google-employees-contractors-announce-union-cwa-alphabet","uuid":"c0c7939d-b0be-408e-97f2-ef45bc8fb36e","parentUuid":"","title":"Google workers announce plans to unionize - The Verge","readerMode":false},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1823,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"presState\":[{\"scroll\":\"0,19281\",\"stateKey\":\"0>html>1\"}],\"persist\":true,\"cacheKey\":0,\"ID\":2,\"url\":\"https:\\\/\\\/www.theverge.com\\\/\",\"title\":\"The Verge\",\"structuredCloneVersion\":8,\"docIdentifier\":3,\"structuredCloneState\":\"AgAAAAAA8f8AAAAACAD\\\/\\\/wAAAAATAP\\\/\\\/\",\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7N2MxMmNkZjItODA1Ny00Nzk2LTkwMmItOTgyZjY0YTRlNjA2fSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7MmJlZWNjOGQtMTUyNy00Yjk1LTg5MmYtZTE1ODMyNTkzNmIzfSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7N2MxMmNkZjItODA1Ny00Nzk2LTkwMmItOTgyZjY0YTRlNjA2fSJ9fQ==\",\"resultPrincipalURI\":null,\"hasUserInteraction\":true,\"originalURI\":\"https:\\\/\\\/www.theverge.com\\\/\",\"docshellUUID\":\"{bb6f2e9a-23b7-4cd1-9521-1c41cf40059d}\"},{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYBAAAAGWh0dHBzOi8vd3d3LnRoZXZlcmdlLmNvbS8AAAAIAQEAAAAZaHR0cHM6Ly93d3cudGhldmVyZ2UuY29tLwEA\",\"persist\":true,\"cacheKey\":0,\"ID\":21,\"csp\":\"CdntGuXUQAS\\\/4CfOuSPZrAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC\\\/\\\/\\\/\\\/\\\/wAAAbsBAAAAGWh0dHBzOi8vd3d3LnRoZXZlcmdlLmNvbS8AAAAAAAAABQAAAAgAAAAQAAAAAP\\\/\\\/\\\/\\\/8AAAAA\\\/\\\/\\\/\\\/\\\/wAAAAgAAAAQAAAAGAAAAAEAAAAYAAAAAQAAABgAAAABAAAAGQAAAAAAAAAA\\\/\\\/\\\/\\\/\\\/wAAAAD\\\/\\\/\\\/\\\/\\\/AAAAAP\\\/\\\/\\\/\\\/8AAAAA\\\/\\\/\\\/\\\/\\\/wEAAAAAAAAAAAAneyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19AAAAAgAAAWsAZABlAGYAYQB1AGwAdAAtAHMAcgBjACAAaAB0AHQAcABzADoAIABkAGEAdABhADoAIAAnAHUAbgBzAGEAZgBlAC0AaQBuAGwAaQBuAGUAJwAgACcAdQBuAHMAYQBmAGUALQBlAHYAYQBsACcAOwAgAGMAaABpAGwAZAAtAHMAcgBjACAAaAB0AHQAcABzADoAIABkAGEAdABhADoAIABiAGwAbwBiADoAOwAgAGMAbwBuAG4AZQBjAHQALQBzAHIAYwAgAGgAdAB0AHAAcwA6ACAAZABhAHQAYQA6ACAAYgBsAG8AYgA6ADsAIABmAG8AbgB0AC0AcwByAGMAIABoAHQAdABwAHMAOgAgAGQAYQB0AGEAOgA7ACAAaQBtAGcALQBzAHIAYwAgAGgAdAB0AHAAcwA6ACAAZABhAHQAYQA6ACAAYgBsAG8AYgA6ADsAIABtAGUAZABpAGEALQBzAHIAYwAgAGgAdAB0AHAAcwA6ACAAZABhAHQAYQA6ACAAYgBsAG8AYgA6ADsAIABvAGIAagBlAGMAdAAtAHMAcgBjACAAaAB0AHQAcABzADoAOwAgAHMAYwByAGkAcAB0AC0AcwByAGMAIABoAHQAdABwAHMAOgAgAGQAYQB0AGEAOgAgAGIAbABvAGIAOgAgACcAdQBuAHMAYQBmAGUALQBpAG4AbABpAG4AZQAnACAAJwB1AG4AcwBhAGYAZQAtAGUAdgBhAGwAJwA7ACAAcwB0AHkAbABlAC0AcwByAGMAIABoAHQAdABwAHMAOgAgACcAdQBuAHMAYQBmAGUALQBpAG4AbABpAG4AZQAnADsAIABiAGwAbwBjAGsALQBhAGwAbAAtAG0AaQB4AGUAZAAtAGMAbwBuAHQAZQBuAHQAOwAgAHUAcABnAHIAYQBkAGUALQBpAG4AcwBlAGMAdQByAGUALQByAGUAcQB1AGUAcwB0AHMAAAAAABkAdQBwAGcAcgBhAGQAZQAtAGkAbgBzAGUAYwB1AHIAZQAtAHIAZQBxAHUAZQBzAHQAcwAB\",\"url\":\"https:\\\/\\\/www.theverge.com\\\/2021\\\/1\\\/4\\\/22212347\\\/google-employees-contractors-announce-union-cwa-alphabet\",\"title\":\"Google workers announce plans to unionize - The Verge\",\"structuredCloneVersion\":8,\"docIdentifier\":22,\"structuredCloneState\":\"AgAAAAAA8f8AAAAACAD\\\/\\\/wAAAAATAP\\\/\\\/\",\"partitionedPrincipalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19\",\"principalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19\",\"resultPrincipalURI\":null,\"hasUserInteraction\":false,\"originalURI\":\"https:\\\/\\\/www.theverge.com\\\/2021\\\/1\\\/4\\\/22212347\\\/google-employees-contractors-announce-union-cwa-alphabet\",\"docshellUUID\":\"{bb6f2e9a-23b7-4cd1-9521-1c41cf40059d}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":2,\"userContextId\":0}}"}},{"session":{"url":"https:\/\/getpocket.com\/explore?src=ff_android&cdn=0","uuid":"b1c1b5b1-3fea-4151-be11-e1989ff4e43c","parentUuid":"","title":"Discover stories on Pocket","readerMode":false},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1823,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"persist\":true,\"cacheKey\":0,\"ID\":32,\"url\":\"https:\\\/\\\/getpocket.com\\\/explore?src=ff_android&cdn=0\",\"title\":\"Discover stories on Pocket\",\"structuredCloneVersion\":8,\"docIdentifier\":34,\"structuredCloneState\":\"AgAAAAAA8f8AAAAACAD\\\/\\\/wMAAIAEAP\\\/\\\/dXJsAAAAAAABAACABAD\\\/\\\/y8AAAAAAAAAAgAAgAQA\\\/\\\/9hcwAAAAAAAB0AAIAEAP\\\/\\\/L2V4cGxvcmU\\\/c3JjPWZmX2FuZHJvaWQmY2RuPTAAAAAHAACABAD\\\/\\\/29wdGlvbnMAAAAAAAgA\\\/\\\/8AAAAAEwD\\\/\\\/wMAAIAEAP\\\/\\\/X19OAAAAAAABAAAAAgD\\\/\\\/wAAAAATAP\\\/\\\/\",\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7MzI3MjYwYzYtYWE2Yi00OTRhLThlNDEtNTU3MzI0MWYzNGRifSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7YWU4ODAzOWUtMzYyZS00ZmZmLWJlZWUtNTMwYTI2ZDczNDUxfSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7MzI3MjYwYzYtYWE2Yi00OTRhLThlNDEtNTU3MzI0MWYzNGRifSJ9fQ==\",\"resultPrincipalURI\":null,\"hasUserInteraction\":true,\"originalURI\":\"https:\\\/\\\/getpocket.com\\\/explore?src=ff_android&cdn=0\",\"docshellUUID\":\"{49f25198-b9b3-4fd0-b347-721ef8af3b93}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":1,\"userContextId\":0}}"}},{"session":{"url":"moz-extension:\/\/c182f1be-1c1b-4305-a329-63268bb4b424\/readerview.html?url=https%3A%2F%2Fwww.pewresearch.org%2Ffact-tank%2F2020%2F12%2F11%2F20-striking-findings-from-2020%2F&id=137438953514","uuid":"ebca3522-5625-4791-9c13-46d0138a375b","parentUuid":"b1c1b5b1-3fea-4151-be11-e1989ff4e43c","title":"20 striking findings from 2020","readerMode":true,"readerModeArticleUrl":"https:\/\/www.pewresearch.org\/fact-tank\/2020\/12\/11\/20-striking-findings-from-2020\/"},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1977,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYBAAAAzWh0dHBzOi8vZ2V0cG9ja2V0LmNvbS9yZWRpcmVjdD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cucGV3cmVzZWFyY2gub3JnJTJGZmFjdC10YW5rJTJGMjAyMCUyRjEyJTJGMTElMkYyMC1zdHJpa2luZy1maW5kaW5ncy1mcm9tLTIwMjAlMkYmaD0zMGUxMjNiMjU0YTk2MGU0NzI0MmNlMmM1ODJiMzgwZjk1ZTkwYzU4MGRiNmE2Yjk0NDdjMzE0NGExMjcyMmZkJm50PTAAAAADAQEAAAAWaHR0cHM6Ly9nZXRwb2NrZXQuY29tLwEA\",\"persist\":true,\"cacheKey\":0,\"ID\":3451829976,\"csp\":\"CdntGuXUQAS\\\/4CfOuSPZrAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC\\\/\\\/\\\/\\\/\\\/wAAAbsBAAAAzWh0dHBzOi8vZ2V0cG9ja2V0LmNvbS9yZWRpcmVjdD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cucGV3cmVzZWFyY2gub3JnJTJGZmFjdC10YW5rJTJGMjAyMCUyRjEyJTJGMTElMkYyMC1zdHJpa2luZy1maW5kaW5ncy1mcm9tLTIwMjAlMkYmaD0zMGUxMjNiMjU0YTk2MGU0NzI0MmNlMmM1ODJiMzgwZjk1ZTkwYzU4MGRiNmE2Yjk0NDdjMzE0NGExMjcyMmZkJm50PTAAAAAAAAAABQAAAAgAAAANAAAAAP\\\/\\\/\\\/\\\/8AAAAA\\\/\\\/\\\/\\\/\\\/wAAAAgAAAANAAAAFQAAALgAAAAVAAAACQAAABUAAAABAAAAFgAAAAgAAAAA\\\/\\\/\\\/\\\/\\\/wAAAAD\\\/\\\/\\\/\\\/\\\/AAAAHwAAAK4AAAAA\\\/\\\/\\\/\\\/\\\/wEAAAAAAAAAAADbeyIxIjp7IjAiOiJodHRwczovL2dldHBvY2tldC5jb20vcmVkaXJlY3Q\\\/dXJsPWh0dHBzJTNBJTJGJTJGd3d3LnBld3Jlc2VhcmNoLm9yZyUyRmZhY3QtdGFuayUyRjIwMjAlMkYxMiUyRjExJTJGMjAtc3RyaWtpbmctZmluZGluZ3MtZnJvbS0yMDIwJTJGJmg9MzBlMTIzYjI1NGE5NjBlNDcyNDJjZTJjNTgyYjM4MGY5NWU5MGM1ODBkYjZhNmI5NDQ3YzMxNDRhMTI3MjJmZCZudD0wIn19AAAAAA==\",\"url\":\"https:\\\/\\\/www.pewresearch.org\\\/fact-tank\\\/2020\\\/12\\\/11\\\/20-striking-findings-from-2020\\\/\",\"title\":\"20 striking findings from 2020 | Pew Research Center\",\"loadReplace\":false,\"docIdentifier\":4,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL2dldHBvY2tldC5jb20vcmVkaXJlY3Q\\\/dXJsPWh0dHBzJTNBJTJGJTJGd3d3LnBld3Jlc2VhcmNoLm9yZyUyRmZhY3QtdGFuayUyRjIwMjAlMkYxMiUyRjExJTJGMjAtc3RyaWtpbmctZmluZGluZ3MtZnJvbS0yMDIwJTJGJmg9MzBlMTIzYjI1NGE5NjBlNDcyNDJjZTJjNTgyYjM4MGY5NWU5MGM1ODBkYjZhNmI5NDQ3YzMxNDRhMTI3MjJmZCZudD0wIn19\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJodHRwczovL2dldHBvY2tldC5jb20vcmVkaXJlY3Q\\\/dXJsPWh0dHBzJTNBJTJGJTJGd3d3LnBld3Jlc2VhcmNoLm9yZyUyRmZhY3QtdGFuayUyRjIwMjAlMkYxMiUyRjExJTJGMjAtc3RyaWtpbmctZmluZGluZ3MtZnJvbS0yMDIwJTJGJmg9MzBlMTIzYjI1NGE5NjBlNDcyNDJjZTJjNTgyYjM4MGY5NWU5MGM1ODBkYjZhNmI5NDQ3YzMxNDRhMTI3MjJmZCZudD0wIn19\",\"principalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL2dldHBvY2tldC5jb20vcmVkaXJlY3Q\\\/dXJsPWh0dHBzJTNBJTJGJTJGd3d3LnBld3Jlc2VhcmNoLm9yZyUyRmZhY3QtdGFuayUyRjIwMjAlMkYxMiUyRjExJTJGMjAtc3RyaWtpbmctZmluZGluZ3MtZnJvbS0yMDIwJTJGJmg9MzBlMTIzYjI1NGE5NjBlNDcyNDJjZTJjNTgyYjM4MGY5NWU5MGM1ODBkYjZhNmI5NDQ3YzMxNDRhMTI3MjJmZCZudD0wIn19\",\"resultPrincipalURI\":\"https:\\\/\\\/www.pewresearch.org\\\/fact-tank\\\/2020\\\/12\\\/11\\\/20-striking-findings-from-2020\\\/\",\"hasUserInteraction\":false,\"originalURI\":\"https:\\\/\\\/www.pewresearch.org\\\/fact-tank\\\/2020\\\/12\\\/11\\\/20-striking-findings-from-2020\\\/\",\"docshellUUID\":\"{f5bfa81e-7e4c-4d07-ae5a-bc0219ca845a}\"},{\"persist\":true,\"cacheKey\":0,\"ID\":4,\"url\":\"moz-extension:\\\/\\\/c182f1be-1c1b-4305-a329-63268bb4b424\\\/readerview.html?url=https%3A%2F%2Fwww.pewresearch.org%2Ffact-tank%2F2020%2F12%2F11%2F20-striking-findings-from-2020%2F&id=137438953514\",\"title\":\"20 striking findings from 2020\",\"docIdentifier\":5,\"partitionedPrincipalToInherit_base64\":\"eyIxIjp7IjAiOiJtb3otZXh0ZW5zaW9uOi8vYzE4MmYxYmUtMWMxYi00MzA1LWEzMjktNjMyNjhiYjRiNDI0L3JlYWRlcnZpZXcuaHRtbD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cucGV3cmVzZWFyY2gub3JnJTJGZmFjdC10YW5rJTJGMjAyMCUyRjEyJTJGMTElMkYyMC1zdHJpa2luZy1maW5kaW5ncy1mcm9tLTIwMjAlMkYmaWQ9MTM3NDM4OTUzNTE0In19\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJtb3otZXh0ZW5zaW9uOi8vYzE4MmYxYmUtMWMxYi00MzA1LWEzMjktNjMyNjhiYjRiNDI0L3JlYWRlcnZpZXcuaHRtbD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cucGV3cmVzZWFyY2gub3JnJTJGZmFjdC10YW5rJTJGMjAyMCUyRjEyJTJGMTElMkYyMC1zdHJpa2luZy1maW5kaW5ncy1mcm9tLTIwMjAlMkYmaWQ9MTM3NDM4OTUzNTE0In19\",\"principalToInherit_base64\":\"eyIxIjp7IjAiOiJtb3otZXh0ZW5zaW9uOi8vYzE4MmYxYmUtMWMxYi00MzA1LWEzMjktNjMyNjhiYjRiNDI0L3JlYWRlcnZpZXcuaHRtbD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cucGV3cmVzZWFyY2gub3JnJTJGZmFjdC10YW5rJTJGMjAyMCUyRjEyJTJGMTElMkYyMC1zdHJpa2luZy1maW5kaW5ncy1mcm9tLTIwMjAlMkYmaWQ9MTM3NDM4OTUzNTE0In19\",\"resultPrincipalURI\":null,\"hasUserInteraction\":false,\"docshellUUID\":\"{f5bfa81e-7e4c-4d07-ae5a-bc0219ca845a}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":2,\"userContextId\":0}}"}}]}
            """.trimIndent()

            val engine = GeckoEngine(context)

            assertTrue(
                getFileForEngine(context, engine).writeString { json },
            )

            val storage = SessionStorage(context, engine)
            val state = storage.restore()

            assertNotNull(state)

            assertEquals(4, state!!.tabs.size)
            assertEquals("b1c1b5b1-3fea-4151-be11-e1989ff4e43c", state.selectedTabId)

            state.tabs[0].state.apply {
                assertEquals("f40b3385-7e75-47b3-9c54-15a02afa27a4", id)
                assertEquals("Fox - Wikipedia", title)
                assertEquals("https://en.m.wikipedia.org/wiki/Fox", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(0, lastAccess)
                assertNotNull(state)
            }

            state.tabs[1].state.apply {
                assertEquals("c0c7939d-b0be-408e-97f2-ef45bc8fb36e", id)
                assertEquals("Google workers announce plans to unionize - The Verge", title)
                assertEquals("https://www.theverge.com/2021/1/4/22212347/google-employees-contractors-announce-union-cwa-alphabet", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(0, lastAccess)
                assertNotNull(state)
            }

            state.tabs[2].state.apply {
                assertEquals("b1c1b5b1-3fea-4151-be11-e1989ff4e43c", id)
                assertEquals("Discover stories on Pocket", title)
                assertEquals("https://getpocket.com/explore?src=ff_android&cdn=0", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(0, lastAccess)
                assertNotNull(state)
            }

            state.tabs[3].state.apply {
                assertEquals("ebca3522-5625-4791-9c13-46d0138a375b", id)
                assertEquals("20 striking findings from 2020", title)
                assertEquals("moz-extension://c182f1be-1c1b-4305-a329-63268bb4b424/readerview.html?url=https%3A%2F%2Fwww.pewresearch.org%2Ffact-tank%2F2020%2F12%2F11%2F20-striking-findings-from-2020%2F&id=137438953514", url)
                assertNull(contextId)
                assertEquals("b1c1b5b1-3fea-4151-be11-e1989ff4e43c", parentId)
                assertTrue(readerState.active)
                assertEquals("https://www.pewresearch.org/fact-tank/2020/12/11/20-striking-findings-from-2020/", readerState.activeUrl)
                assertEquals(0, lastAccess)
                assertNotNull(state)
            }
        }
    }

    /**
     * App: Firefox for Android (Fenix)
     * Version: 83.1.0
     */
    @Test
    fun Fenix_83_1_0() {
        runBlocking(Dispatchers.Main) {
            val json = """
                {"version":2,"selectedTabId":"cbea9370-719e-47ef-931b-dee03f15761f","sessionStateTuples":[{"session":{"url":"https://byo.com/projects/","uuid":"ae4e089e-7efe-4f7f-a2c5-290308119f64","parentUuid":"","title":"Projects - Brew Your Own","contextId":null,"readerMode":false,"lastAccess":1609762083437},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1823,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"persist\":true,\"cacheKey\":0,\"ID\":1,\"url\":\"https:\\/\\/byo.com\\/\",\"title\":\"Home - Brew Your Own\",\"loadReplace\":true,\"docIdentifier\":2147483650,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7YTBmMjQ5NDAtNWU3Mi00OGQ0LWI4ZTAtODE1YzMyY2MyMTQyfSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7Njc0NmEwYjYtNzIyMi00YmRkLTllZjUtZDQ5Mjk0ZDUxYjIxfSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7YTBmMjQ5NDAtNWU3Mi00OGQ0LWI4ZTAtODE1YzMyY2MyMTQyfSJ9fQ==\",\"resultPrincipalURI\":\"https:\\/\\/byo.com\\/\",\"hasUserInteraction\":true,\"originalURI\":\"http:\\/\\/byo.com\\/\",\"docshellUUID\":\"{fa2256f5-042f-4203-900b-3ec7bedad886}\"},{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYBAAAAEGh0dHBzOi8vYnlvLmNvbS8AAAAFAQEAAAAQaHR0cHM6Ly9ieW8uY29tLwEA\",\"persist\":true,\"cacheKey\":0,\"ID\":8,\"csp\":\"CdntGuXUQAS\\/4CfOuSPZrAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC\\/\\/\\/\\/\\/wAAAbsBAAAAEGh0dHBzOi8vYnlvLmNvbS8AAAAAAAAABQAAAAgAAAAHAAAAAP\\/\\/\\/\\/8AAAAA\\/\\/\\/\\/\\/wAAAAgAAAAHAAAADwAAAAEAAAAPAAAAAQAAAA8AAAABAAAAEAAAAAAAAAAA\\/\\/\\/\\/\\/wAAAAD\\/\\/\\/\\/\\/AAAAAP\\/\\/\\/\\/8AAAAA\\/\\/\\/\\/\\/wEAAAAAAAAAAAAeeyIxIjp7IjAiOiJodHRwczovL2J5by5jb20vIn19AAAAAA==\",\"url\":\"https:\\/\\/byo.com\\/projects\\/\",\"title\":\"Projects - Brew Your Own\",\"loadReplace\":false,\"docIdentifier\":2147483657,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL2J5by5jb20vIn19\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJodHRwczovL2J5by5jb20vIn19\",\"principalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL2J5by5jb20vIn19\",\"resultPrincipalURI\":\"https:\\/\\/byo.com\\/projects\\/\",\"hasUserInteraction\":false,\"originalURI\":\"https:\\/\\/byo.com\\/projects\\/\",\"docshellUUID\":\"{fa2256f5-042f-4203-900b-3ec7bedad886}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":2,\"userContextId\":0}}"}},{"session":{"url":"https://www.theverge.com/2021/1/3/22206649/samsung-officially-confirms-galaxy-s21-event-on-january-14th","uuid":"cbea9370-719e-47ef-931b-dee03f15761f","parentUuid":"","title":"Samsung officially confirms Galaxy S21 event for January 14th","contextId":null,"readerMode":true,"lastAccess":1609762120069,"readerModeArticleUrl":"https://www.theverge.com/2021/1/3/22206649/samsung-officially-confirms-galaxy-s21-event-on-january-14th"},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1977,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"persist\":true,\"cacheKey\":0,\"ID\":3444447746,\"url\":\"https:\\/\\/www.theverge.com\\/\",\"title\":\"The Verge\",\"structuredCloneVersion\":8,\"docIdentifier\":4,\"structuredCloneState\":\"AgAAAAAA8f8AAAAACAD\\/\\/wAAAAATAP\\/\\/\",\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7NGJlMDM3YmUtNGZiZC00YjJkLTg4YjUtYWNiMWQ5YjZlMjY1fSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7NzJhNDE1ZDEtOWM5My00M2Y2LTg3NWQtMGRhNzgxMGUzMjhmfSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7NGJlMDM3YmUtNGZiZC00YjJkLTg4YjUtYWNiMWQ5YjZlMjY1fSJ9fQ==\",\"resultPrincipalURI\":null,\"hasUserInteraction\":true,\"originalURI\":\"https:\\/\\/www.theverge.com\\/\",\"docshellUUID\":\"{cd02153d-b7f7-429d-bce3-8f2fd2005ea4}\"},{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYBAAAAGWh0dHBzOi8vd3d3LnRoZXZlcmdlLmNvbS8AAAAIAQEAAAAZaHR0cHM6Ly93d3cudGhldmVyZ2UuY29tLwEA\",\"persist\":true,\"cacheKey\":0,\"ID\":3444447767,\"csp\":\"CdntGuXUQAS\\/4CfOuSPZrAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC\\/\\/\\/\\/\\/wAAAbsBAAAAGWh0dHBzOi8vd3d3LnRoZXZlcmdlLmNvbS8AAAAAAAAABQAAAAgAAAAQAAAAAP\\/\\/\\/\\/8AAAAA\\/\\/\\/\\/\\/wAAAAgAAAAQAAAAGAAAAAEAAAAYAAAAAQAAABgAAAABAAAAGQAAAAAAAAAA\\/\\/\\/\\/\\/wAAAAD\\/\\/\\/\\/\\/AAAAAP\\/\\/\\/\\/8AAAAA\\/\\/\\/\\/\\/wEAAAAAAAAAAAAneyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19AAAAAgAAAWsAZABlAGYAYQB1AGwAdAAtAHMAcgBjACAAaAB0AHQAcABzADoAIABkAGEAdABhADoAIAAnAHUAbgBzAGEAZgBlAC0AaQBuAGwAaQBuAGUAJwAgACcAdQBuAHMAYQBmAGUALQBlAHYAYQBsACcAOwAgAGMAaABpAGwAZAAtAHMAcgBjACAAaAB0AHQAcABzADoAIABkAGEAdABhADoAIABiAGwAbwBiADoAOwAgAGMAbwBuAG4AZQBjAHQALQBzAHIAYwAgAGgAdAB0AHAAcwA6ACAAZABhAHQAYQA6ACAAYgBsAG8AYgA6ADsAIABmAG8AbgB0AC0AcwByAGMAIABoAHQAdABwAHMAOgAgAGQAYQB0AGEAOgA7ACAAaQBtAGcALQBzAHIAYwAgAGgAdAB0AHAAcwA6ACAAZABhAHQAYQA6ACAAYgBsAG8AYgA6ADsAIABtAGUAZABpAGEALQBzAHIAYwAgAGgAdAB0AHAAcwA6ACAAZABhAHQAYQA6ACAAYgBsAG8AYgA6ADsAIABvAGIAagBlAGMAdAAtAHMAcgBjACAAaAB0AHQAcABzADoAOwAgAHMAYwByAGkAcAB0AC0AcwByAGMAIABoAHQAdABwAHMAOgAgAGQAYQB0AGEAOgAgAGIAbABvAGIAOgAgACcAdQBuAHMAYQBmAGUALQBpAG4AbABpAG4AZQAnACAAJwB1AG4AcwBhAGYAZQAtAGUAdgBhAGwAJwA7ACAAcwB0AHkAbABlAC0AcwByAGMAIABoAHQAdABwAHMAOgAgACcAdQBuAHMAYQBmAGUALQBpAG4AbABpAG4AZQAnADsAIABiAGwAbwBjAGsALQBhAGwAbAAtAG0AaQB4AGUAZAAtAGMAbwBuAHQAZQBuAHQAOwAgAHUAcABnAHIAYQBkAGUALQBpAG4AcwBlAGMAdQByAGUALQByAGUAcQB1AGUAcwB0AHMAAAAAABkAdQBwAGcAcgBhAGQAZQAtAGkAbgBzAGUAYwB1AHIAZQAtAHIAZQBxAHUAZQBzAHQAcwAB\",\"url\":\"https:\\/\\/www.theverge.com\\/2021\\/1\\/3\\/22206649\\/samsung-officially-confirms-galaxy-s21-event-on-january-14th\",\"title\":\"Samsung officially confirms Galaxy S21 event for January 14th - The Verge\",\"structuredCloneVersion\":8,\"docIdentifier\":5,\"structuredCloneState\":\"AgAAAAAA8f8AAAAACAD\\/\\/wAAAAATAP\\/\\/\",\"partitionedPrincipalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19\",\"principalToInherit_base64\":\"eyIxIjp7IjAiOiJodHRwczovL3d3dy50aGV2ZXJnZS5jb20vIn19\",\"resultPrincipalURI\":null,\"hasUserInteraction\":false,\"originalURI\":\"https:\\/\\/www.theverge.com\\/2021\\/1\\/3\\/22206649\\/samsung-officially-confirms-galaxy-s21-event-on-january-14th\",\"docshellUUID\":\"{cd02153d-b7f7-429d-bce3-8f2fd2005ea4}\"},{\"persist\":true,\"cacheKey\":0,\"ID\":5,\"url\":\"moz-extension:\\/\\/de686481-5025-4447-9228-8cc85aa1acbd\\/readerview.html?url=https%3A%2F%2Fwww.theverge.com%2F2021%2F1%2F3%2F22206649%2Fsamsung-officially-confirms-galaxy-s21-event-on-january-14th&id=137438953499\",\"title\":\"Samsung officially confirms Galaxy S21 event for January 14th\",\"docIdentifier\":6,\"partitionedPrincipalToInherit_base64\":\"eyIxIjp7IjAiOiJtb3otZXh0ZW5zaW9uOi8vZGU2ODY0ODEtNTAyNS00NDQ3LTkyMjgtOGNjODVhYTFhY2JkL3JlYWRlcnZpZXcuaHRtbD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cudGhldmVyZ2UuY29tJTJGMjAyMSUyRjElMkYzJTJGMjIyMDY2NDklMkZzYW1zdW5nLW9mZmljaWFsbHktY29uZmlybXMtZ2FsYXh5LXMyMS1ldmVudC1vbi1qYW51YXJ5LTE0dGgmaWQ9MTM3NDM4OTUzNDk5In19\",\"triggeringPrincipal_base64\":\"eyIxIjp7IjAiOiJtb3otZXh0ZW5zaW9uOi8vZGU2ODY0ODEtNTAyNS00NDQ3LTkyMjgtOGNjODVhYTFhY2JkL3JlYWRlcnZpZXcuaHRtbD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cudGhldmVyZ2UuY29tJTJGMjAyMSUyRjElMkYzJTJGMjIyMDY2NDklMkZzYW1zdW5nLW9mZmljaWFsbHktY29uZmlybXMtZ2FsYXh5LXMyMS1ldmVudC1vbi1qYW51YXJ5LTE0dGgmaWQ9MTM3NDM4OTUzNDk5In19\",\"principalToInherit_base64\":\"eyIxIjp7IjAiOiJtb3otZXh0ZW5zaW9uOi8vZGU2ODY0ODEtNTAyNS00NDQ3LTkyMjgtOGNjODVhYTFhY2JkL3JlYWRlcnZpZXcuaHRtbD91cmw9aHR0cHMlM0ElMkYlMkZ3d3cudGhldmVyZ2UuY29tJTJGMjAyMSUyRjElMkYzJTJGMjIyMDY2NDklMkZzYW1zdW5nLW9mZmljaWFsbHktY29uZmlybXMtZ2FsYXh5LXMyMS1ldmVudC1vbi1qYW51YXJ5LTE0dGgmaWQ9MTM3NDM4OTUzNDk5In19\",\"resultPrincipalURI\":null,\"hasUserInteraction\":false,\"docshellUUID\":\"{cd02153d-b7f7-429d-bce3-8f2fd2005ea4}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":3,\"userContextId\":0}}"}}]}
            """.trimIndent()

            val engine = GeckoEngine(context)

            assertTrue(
                getFileForEngine(context, engine).writeString { json },
            )

            val storage = SessionStorage(context, engine)
            val state = storage.restore()

            assertNotNull(state)

            assertEquals(2, state!!.tabs.size)
            assertEquals("cbea9370-719e-47ef-931b-dee03f15761f", state.selectedTabId)

            state.tabs[0].state.apply {
                assertEquals("ae4e089e-7efe-4f7f-a2c5-290308119f64", id)
                assertEquals("Projects - Brew Your Own", title)
                assertEquals("https://byo.com/projects/", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(1609762083437, lastAccess)
                assertNotNull(state)
            }

            state.tabs[1].state.apply {
                assertEquals("cbea9370-719e-47ef-931b-dee03f15761f", id)
                assertEquals("Samsung officially confirms Galaxy S21 event for January 14th", title)
                assertEquals("https://www.theverge.com/2021/1/3/22206649/samsung-officially-confirms-galaxy-s21-event-on-january-14th", url)
                assertNull(contextId)
                assertNull(parentId)
                assertTrue(readerState.active)
                assertEquals("https://www.theverge.com/2021/1/3/22206649/samsung-officially-confirms-galaxy-s21-event-on-january-14th", readerState.activeUrl)
                assertEquals(1609762120069, lastAccess)
                assertNotNull(state)
            }
        }
    }

    /**
     * App: Firefox for Android (Fenix)
     * Version: master branch (GeckoView Nightly), 2021-01-05
     */
    @Test
    fun Fenix_Master_2021_01_5() {
        runBlocking(Dispatchers.Main) {
            val json = """
                {"version":2,"selectedTabId":"7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f","sessionStateTuples":[{"session":{"url":"https://airhorner.com/","uuid":"0e556bb8-9120-48af-bc93-2bba0d4ec346","parentUuid":"","title":"The Air Horner","contextId":null,"readerMode":false,"lastAccess":1609784156118},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1823,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"persist\":true,\"cacheKey\":0,\"ID\":0,\"url\":\"https:\\/\\/airhorner.com\\/\",\"title\":\"The Air Horner\",\"loadReplace\":true,\"docIdentifier\":2147483649,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7YjRjZTU0NjItMGQ0Yy00MDc3LTgwMzctYjk2YTI5MGEyMDRifSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7NmE1YzZhODItODdlNy00ODQ0LTljMWItYjY0ZWRiZTA1MDY1fSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7YjRjZTU0NjItMGQ0Yy00MDc3LTgwMzctYjk2YTI5MGEyMDRifSJ9fQ==\",\"resultPrincipalURI\":\"https:\\/\\/airhorner.com\\/\",\"hasUserInteraction\":false,\"originalURI\":\"http:\\/\\/airhorner.com\\/\",\"docshellUUID\":\"{16bc3b14-e47f-4839-b809-095f0b0f79b4}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":1,\"userContextId\":0}}"}},{"session":{"url":"https://www.wikipedia.org/","uuid":"7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f","parentUuid":"","title":"","contextId":null,"readerMode":false,"lastAccess":0},"engineSession":{}}]}
            """.trimIndent()

            val engine = GeckoEngine(context)

            assertTrue(
                getFileForEngine(context, engine).writeString { json },
            )

            val storage = SessionStorage(context, engine)
            val state = storage.restore()

            assertNotNull(state)

            assertEquals(2, state!!.tabs.size)
            assertEquals("7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f", state.selectedTabId)

            state.tabs[0].state.apply {
                assertEquals("0e556bb8-9120-48af-bc93-2bba0d4ec346", id)
                assertEquals("The Air Horner", title)
                assertEquals("https://airhorner.com/", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(1609784156118, lastAccess)
                assertNotNull(state)
            }

            state.tabs[1].state.apply {
                assertEquals("7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f", id)
                assertEquals("", title)
                assertEquals("https://www.wikipedia.org/", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(0, lastAccess)
                assertNotNull(state)
            }
        }
    }

    /**
     * App: Firefox for Android (Fenix)
     * Version: master branch (GeckoView Nightly), 2021-01-08
     */
    @Test
    fun Fenix_Master_2021_01_8() {
        runBlocking(Dispatchers.Main) {
            val json = """
                {"version":2,"selectedTabId":"7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f","sessionStateTuples":[{"session":{"url":"https://airhorner.com/","uuid":"0e556bb8-9120-48af-bc93-2bba0d4ec346","parentUuid":"","title":"The Air Horner","contextId":null,"readerMode":false,"lastAccess":1609784156118, "createdAt":1609784156118},"engineSession":{"GECKO_STATE":"{\"scrolldata\":{\"zoom\":{\"resolution\":1,\"displaySize\":{\"height\":1823,\"width\":1080}}},\"history\":{\"entries\":[{\"referrerInfo\":\"BBoSnxDOS9qmDeAnom1e0AAAAAAAAAAAwAAAAAAAAEYAAAAAAAEBAAAAAAEA\",\"persist\":true,\"cacheKey\":0,\"ID\":0,\"url\":\"https:\\/\\/airhorner.com\\/\",\"title\":\"The Air Horner\",\"loadReplace\":true,\"docIdentifier\":2147483649,\"loadReplace2\":true,\"partitionedPrincipalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7YjRjZTU0NjItMGQ0Yy00MDc3LTgwMzctYjk2YTI5MGEyMDRifSJ9fQ==\",\"triggeringPrincipal_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7NmE1YzZhODItODdlNy00ODQ0LTljMWItYjY0ZWRiZTA1MDY1fSJ9fQ==\",\"principalToInherit_base64\":\"eyIwIjp7IjAiOiJtb3otbnVsbHByaW5jaXBhbDp7YjRjZTU0NjItMGQ0Yy00MDc3LTgwMzctYjk2YTI5MGEyMDRifSJ9fQ==\",\"resultPrincipalURI\":\"https:\\/\\/airhorner.com\\/\",\"hasUserInteraction\":false,\"originalURI\":\"http:\\/\\/airhorner.com\\/\",\"docshellUUID\":\"{16bc3b14-e47f-4839-b809-095f0b0f79b4}\"}],\"requestedIndex\":0,\"fromIdx\":-1,\"index\":1,\"userContextId\":0}}"}},{"session":{"url":"https://www.wikipedia.org/","uuid":"7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f","parentUuid":"","title":"","contextId":null,"readerMode":false,"lastAccess":0},"engineSession":{}}]}
            """.trimIndent()

            val engine = GeckoEngine(context)

            assertTrue(
                getFileForEngine(context, engine).writeString { json },
            )

            val storage = SessionStorage(context, engine)
            val state = storage.restore()

            assertNotNull(state)

            assertEquals(2, state!!.tabs.size)
            assertEquals("7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f", state.selectedTabId)

            state.tabs[0].state.apply {
                assertEquals("0e556bb8-9120-48af-bc93-2bba0d4ec346", id)
                assertEquals("The Air Horner", title)
                assertEquals("https://airhorner.com/", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(1609784156118, lastAccess)
                assertEquals(1609784156118, createdAt)
                assertNotNull(state)
            }

            state.tabs[1].state.apply {
                assertEquals("7f4fd2c9-2bb1-4c23-a06c-7fbd2b78922f", id)
                assertEquals("", title)
                assertEquals("https://www.wikipedia.org/", url)
                assertNull(contextId)
                assertNull(parentId)
                assertFalse(readerState.active)
                assertNull(readerState.activeUrl)
                assertEquals(0, lastAccess)
                assertEquals(0, createdAt)
                assertNotNull(state)
            }
        }
    }
}
