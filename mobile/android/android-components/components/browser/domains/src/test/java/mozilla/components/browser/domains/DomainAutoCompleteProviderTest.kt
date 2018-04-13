/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import android.preference.PreferenceManager
import mozilla.components.browser.domains.DomainAutoCompleteProvider.AutocompleteSource
import mozilla.components.browser.domains.DomainAutoCompleteProvider.Domain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(constants = BuildConfig::class)
class DomainAutoCompleteProviderTest {

    @After
    fun tearDown() {
        PreferenceManager.getDefaultSharedPreferences(RuntimeEnvironment.application)
                .edit()
                .clear()
                .apply()
    }

    @Test
    fun testDomainCreation() {
        val firstItem = Domain.create("https://mozilla.com")

        assertTrue(firstItem.protocol == "https://")
        assertFalse(firstItem.hasWww)
        assertTrue(firstItem.host == "mozilla.com")

        val secondItem = Domain.create("www.mozilla.com")

        assertTrue(secondItem.protocol == "http://")
        assertTrue(secondItem.hasWww)
        assertTrue(secondItem.host == "mozilla.com")
    }

    @Test(expected = IllegalStateException::class)
    fun testDomainCreationWithBadURLThrowsException() {
        Domain.create("http://www.")
    }

    @Test
    fun testAutocompletionWithShippedDomains() {
        val provider = DomainAutoCompleteProvider()
        provider.initialize(RuntimeEnvironment.application, true, false, false)

        val domains = listOf("mozilla.org", "google.com", "facebook.com")
        provider.onDomainsLoaded(domains, emptyList())

        assertCompletion(provider, "m", AutocompleteSource.DEFAULT_LIST, domains.size, "mozilla.org")
        assertCompletion(provider, "www", AutocompleteSource.DEFAULT_LIST, domains.size, "www.mozilla.org")
        assertCompletion(provider, "www.face", AutocompleteSource.DEFAULT_LIST, domains.size, "www.facebook.com")
        assertCompletion(provider, "MOZ", AutocompleteSource.DEFAULT_LIST, domains.size, "MOZilla.org")
        assertCompletion(provider, "www.GOO", AutocompleteSource.DEFAULT_LIST, domains.size, "www.GOOgle.com")
        assertCompletion(provider, "WWW.GOOGLE.", AutocompleteSource.DEFAULT_LIST, domains.size, "WWW.GOOGLE.com")
        assertCompletion(provider, "www.facebook.com", AutocompleteSource.DEFAULT_LIST, domains.size, "www.facebook.com")
        assertCompletion(provider, "facebook.com", AutocompleteSource.DEFAULT_LIST, domains.size, "facebook.com")

        assertNoCompletion(provider, "wwww")
        assertNoCompletion(provider, "yahoo")
    }

    @Test
    fun testAutocompletionWithCustomDomains() {
        val domains = listOf("facebook.com", "google.com", "mozilla.org")
        val customDomains = listOf("gap.com", "www.fanfiction.com", "https://mobile.de")

        val provider = DomainAutoCompleteProvider()
        provider.initialize(RuntimeEnvironment.application, true, true, false)
        provider.onDomainsLoaded(domains, customDomains)

        assertCompletion(provider, "f", AutocompleteSource.CUSTOM_LIST, customDomains.size, "fanfiction.com", hasWww = true)
        assertCompletion(provider, "fa", AutocompleteSource.CUSTOM_LIST, customDomains.size, "fanfiction.com", hasWww = true)
        assertCompletion(provider, "fac", AutocompleteSource.DEFAULT_LIST, domains.size, "facebook.com")

        assertCompletion(provider, "g", AutocompleteSource.CUSTOM_LIST, customDomains.size, "gap.com")
        assertCompletion(provider, "go", AutocompleteSource.DEFAULT_LIST, domains.size, "google.com")
        assertCompletion(provider, "ga", AutocompleteSource.CUSTOM_LIST, customDomains.size, "gap.com")

        assertCompletion(provider, "m", AutocompleteSource.CUSTOM_LIST, customDomains.size, "mobile.de", protocol = "https://")
        assertCompletion(provider, "mo", AutocompleteSource.CUSTOM_LIST, customDomains.size, "mobile.de", protocol = "https://")
        assertCompletion(provider, "mob", AutocompleteSource.CUSTOM_LIST, customDomains.size, "mobile.de", protocol = "https://")
        assertCompletion(provider, "moz", AutocompleteSource.DEFAULT_LIST, domains.size, "mozilla.org")
    }

    @Test
    fun testAutocompletionWithoutDomains() {
        val filter = DomainAutoCompleteProvider()
        assertNoCompletion(filter, "mozilla")
    }

    private fun assertCompletion(
        provider: DomainAutoCompleteProvider,
        text: String,
        domainSource: String,
        sourceSize: Int,
        completion: String,
        protocol: String = "http://",
        hasWww: Boolean = false
    ) {
        val resultCallback = { result: String, domain: Domain?, source: String, totalItems: Int ->
            assertFalse(result.isEmpty())

            assertEquals(completion, result)
            assertEquals(protocol, domain?.protocol)
            assertEquals(hasWww, domain?.hasWww)
            assertEquals(domainSource, source)
            assertEquals(sourceSize, totalItems)
        }
        provider.autocomplete(text, resultCallback)
    }

    private fun assertNoCompletion(provider: DomainAutoCompleteProvider, text: String) {
        val resultCallback = { result: String, domain: Domain?, source: String, totalItems: Int ->
            assertTrue(result.isEmpty())
            assertNull(domain)
            assertTrue(source.isEmpty())
            assertEquals(0, totalItems)
        }
        provider.autocomplete(text, resultCallback)
    }
}
