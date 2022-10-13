/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import mozilla.components.browser.domains.autocomplete.BaseDomainAutocompleteProvider
import mozilla.components.browser.domains.autocomplete.CustomDomainsProvider
import mozilla.components.browser.domains.autocomplete.DomainList
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Test

class ProvidersTest {
    @Test
    fun autocompletionWithShippedDomains() {
        val provider = ShippedDomainsProvider()
        provider.domains = listOf("mozilla.org", "google.com", "facebook.com").into()

        val size = provider.domains.size

        assertCompletion(provider, "m", DomainList.DEFAULT, size, "mozilla.org", "http://mozilla.org")
        assertCompletion(provider, "www", DomainList.DEFAULT, size, "www.mozilla.org", "http://mozilla.org")
        assertCompletion(provider, "www.face", DomainList.DEFAULT, size, "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, "MOZ", DomainList.DEFAULT, size, "MOZilla.org", "http://mozilla.org")
        assertCompletion(provider, "www.GOO", DomainList.DEFAULT, size, "www.GOOgle.com", "http://google.com")
        assertCompletion(provider, "WWW.GOOGLE.", DomainList.DEFAULT, size, "WWW.GOOGLE.com", "http://google.com")
        assertCompletion(provider, "www.facebook.com", DomainList.DEFAULT, size, "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, "facebook.com", DomainList.DEFAULT, size, "facebook.com", "http://facebook.com")

        assertNoCompletion(provider, "wwww")
        assertNoCompletion(provider, "yahoo")
    }

    @Test
    fun autocompletionWithCustomDomains() {
        val customDomains = listOf("gap.com", "www.fanfiction.com", "https://mobile.de")

        val provider = CustomDomainsProvider()
        provider.domains = customDomains.into()

        assertCompletion(provider, "f", DomainList.CUSTOM, customDomains.size, "fanfiction.com", "http://www.fanfiction.com")
        assertCompletion(provider, "fa", DomainList.CUSTOM, customDomains.size, "fanfiction.com", "http://www.fanfiction.com")

        assertCompletion(provider, "g", DomainList.CUSTOM, customDomains.size, "gap.com", "http://gap.com")
        assertCompletion(provider, "ga", DomainList.CUSTOM, customDomains.size, "gap.com", "http://gap.com")

        assertCompletion(provider, "m", DomainList.CUSTOM, customDomains.size, "mobile.de", "https://mobile.de")
        assertCompletion(provider, "mo", DomainList.CUSTOM, customDomains.size, "mobile.de", "https://mobile.de")
        assertCompletion(provider, "mob", DomainList.CUSTOM, customDomains.size, "mobile.de", "https://mobile.de")
    }

    @Test
    fun autocompletionWithoutDomains() {
        val filter = CustomDomainsProvider()
        assertNoCompletion(filter, "mozilla")
    }

    private fun assertCompletion(
        provider: BaseDomainAutocompleteProvider,
        text: String,
        domainSource: DomainList,
        sourceSize: Int,
        completion: String,
        expectedUrl: String,
    ) {
        val result = provider.getAutocompleteSuggestion(text)!!
        assertFalse(result.text.isEmpty())

        assertEquals(completion, result.text)
        assertEquals(domainSource.listName, result.source)
        assertEquals(expectedUrl, result.url)
        assertEquals(sourceSize, result.totalItems)
    }

    private fun assertNoCompletion(provider: BaseDomainAutocompleteProvider, text: String) {
        assertNull(provider.getAutocompleteSuggestion(text))
    }
}
