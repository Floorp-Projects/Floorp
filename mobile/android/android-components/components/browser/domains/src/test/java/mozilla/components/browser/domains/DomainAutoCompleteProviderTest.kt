/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.browser.domains

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.domains.DomainAutoCompleteProvider.AutocompleteSource.CUSTOM_LIST
import mozilla.components.browser.domains.DomainAutoCompleteProvider.AutocompleteSource.DEFAULT_LIST
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * While [DomainAutoCompleteProvider] exists (even if it's deprecated) we need to test it.
 */
@RunWith(AndroidJUnit4::class)
class DomainAutoCompleteProviderTest {

    @Test
    fun autocompletionWithShippedDomains() {
        val provider = DomainAutoCompleteProvider().also {
            it.initialize(
                testContext,
                useShippedDomains = true,
                useCustomDomains = false,
                loadDomainsFromDisk = false,
            )

            it.shippedDomains = listOf("mozilla.org", "google.com", "facebook.com").into()
            it.customDomains = emptyList()
        }

        val size = provider.shippedDomains.size

        assertCompletion(provider, "m", DEFAULT_LIST, size, "mozilla.org", "http://mozilla.org")
        assertCompletion(provider, "www", DEFAULT_LIST, size, "www.mozilla.org", "http://mozilla.org")
        assertCompletion(provider, "www.face", DEFAULT_LIST, size, "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, "MOZ", DEFAULT_LIST, size, "MOZilla.org", "http://mozilla.org")
        assertCompletion(provider, "www.GOO", DEFAULT_LIST, size, "www.GOOgle.com", "http://google.com")
        assertCompletion(provider, "WWW.GOOGLE.", DEFAULT_LIST, size, "WWW.GOOGLE.com", "http://google.com")
        assertCompletion(provider, "www.facebook.com", DEFAULT_LIST, size, "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, "facebook.com", DEFAULT_LIST, size, "facebook.com", "http://facebook.com")

        assertNoCompletion(provider, "wwww")
        assertNoCompletion(provider, "yahoo")
    }

    @Test
    fun autocompletionWithCustomDomains() {
        val domains = listOf("facebook.com", "google.com", "mozilla.org")
        val customDomains = listOf("gap.com", "www.fanfiction.com", "https://mobile.de")

        val provider = DomainAutoCompleteProvider().also {
            it.initialize(
                testContext,
                useShippedDomains = true,
                useCustomDomains = true,
                loadDomainsFromDisk = false,
            )
            it.shippedDomains = domains.into()
            it.customDomains = customDomains.into()
        }

        assertCompletion(provider, "f", CUSTOM_LIST, customDomains.size, "fanfiction.com", "http://www.fanfiction.com")
        assertCompletion(provider, "fa", CUSTOM_LIST, customDomains.size, "fanfiction.com", "http://www.fanfiction.com")
        assertCompletion(provider, "fac", DEFAULT_LIST, domains.size, "facebook.com", "http://facebook.com")

        assertCompletion(provider, "g", CUSTOM_LIST, customDomains.size, "gap.com", "http://gap.com")
        assertCompletion(provider, "go", DEFAULT_LIST, domains.size, "google.com", "http://google.com")
        assertCompletion(provider, "ga", CUSTOM_LIST, customDomains.size, "gap.com", "http://gap.com")

        assertCompletion(provider, "m", CUSTOM_LIST, customDomains.size, "mobile.de", "https://mobile.de")
        assertCompletion(provider, "mo", CUSTOM_LIST, customDomains.size, "mobile.de", "https://mobile.de")
        assertCompletion(provider, "mob", CUSTOM_LIST, customDomains.size, "mobile.de", "https://mobile.de")
        assertCompletion(provider, "moz", DEFAULT_LIST, domains.size, "mozilla.org", "http://mozilla.org")
    }

    @Test
    fun autocompletionWithoutDomains() {
        val filter = DomainAutoCompleteProvider()
        assertNoCompletion(filter, "mozilla")
    }

    private fun assertCompletion(
        provider: DomainAutoCompleteProvider,
        text: String,
        domainSource: String,
        sourceSize: Int,
        completion: String,
        expectedUrl: String,
    ) {
        val result = provider.autocomplete(text)

        assertFalse(result.text.isEmpty())

        assertEquals(completion, result.text)
        assertEquals(domainSource, result.source)
        assertEquals(expectedUrl, result.url)
        assertEquals(sourceSize, result.size)
    }

    private fun assertNoCompletion(provider: DomainAutoCompleteProvider, text: String) {
        val result = provider.autocomplete(text)

        assertTrue(result.text.isEmpty())
        assertTrue(result.url.isEmpty())
        assertTrue(result.source.isEmpty())
        assertEquals(0, result.size)
    }
}
