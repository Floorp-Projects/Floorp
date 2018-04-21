/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import android.preference.PreferenceManager
import mozilla.components.browser.domains.DomainAutoCompleteProvider.AutocompleteSource.CUSTOM_LIST
import mozilla.components.browser.domains.DomainAutoCompleteProvider.AutocompleteSource.DEFAULT_LIST
import mozilla.components.browser.domains.DomainAutoCompleteProvider.Domain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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

    @Test
    fun testDomainCanCreateUrl() {
        val firstItem = Domain.create("https://mozilla.com")
        assertEquals("https://mozilla.com", firstItem.url)

        val secondItem = Domain.create("www.mozilla.com")
        assertEquals("http://www.mozilla.com", secondItem.url)
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

        val size = domains.size

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
    fun testAutocompletionWithCustomDomains() {
        val domains = listOf("facebook.com", "google.com", "mozilla.org")
        val customDomains = listOf("gap.com", "www.fanfiction.com", "https://mobile.de")

        val provider = DomainAutoCompleteProvider()
        provider.initialize(RuntimeEnvironment.application, true, true, false)
        provider.onDomainsLoaded(domains, customDomains)

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
        expectedUrl: String
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
