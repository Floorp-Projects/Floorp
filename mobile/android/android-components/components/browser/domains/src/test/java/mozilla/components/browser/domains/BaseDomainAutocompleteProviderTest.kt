/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import android.content.Context
import android.os.Looper.getMainLooper
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import mozilla.components.browser.domains.autocomplete.BaseDomainAutocompleteProvider
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.domains.autocomplete.DomainList
import mozilla.components.browser.domains.autocomplete.DomainsLoader
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class BaseDomainAutocompleteProviderTest {

    @Test
    fun `empty provider with DEFAULT list returns nothing`() {
        val provider = createAndInitProvider(testContext, DomainList.DEFAULT) {
            emptyList()
        }

        assertNoCompletion(provider, "m")
        assertNoCompletion(provider, "mo")
        assertNoCompletion(provider, "moz")
        assertNoCompletion(provider, "g")
        assertNoCompletion(provider, "go")
        assertNoCompletion(provider, "goo")
        assertNoCompletion(provider, "w")
        assertNoCompletion(provider, "www")
    }

    @Test
    fun `empty provider with CUSTOM list returns nothing`() {
        val provider = createAndInitProvider(testContext, DomainList.CUSTOM) {
            emptyList()
        }

        assertNoCompletion(provider, "m")
        assertNoCompletion(provider, "mo")
        assertNoCompletion(provider, "moz")
        assertNoCompletion(provider, "g")
        assertNoCompletion(provider, "go")
        assertNoCompletion(provider, "goo")
        assertNoCompletion(provider, "w")
        assertNoCompletion(provider, "www")
    }

    @Test
    fun `non-empty provider with DEFAULT list returns completion`() {
        val domains = listOf("mozilla.org", "google.com", "facebook.com").into()
        val list = DomainList.DEFAULT
        val domainsCount = domains.size

        val provider = createAndInitProvider(testContext, list) { domains }
        shadowOf(getMainLooper()).idle()

        assertCompletion(provider, list, domainsCount, "m", "m", "mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "moz", "moz", "mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "www", "www", "www.mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "www.face", "www.face", "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, list, domainsCount, "M", "m", "Mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "MOZ", "moz", "MOZilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "www.GOO", "www.goo", "www.GOOgle.com", "http://google.com")
        assertCompletion(provider, list, domainsCount, "WWW.GOOGLE.", "www.google.", "WWW.GOOGLE.com", "http://google.com")
        assertCompletion(provider, list, domainsCount, "www.facebook.com", "www.facebook.com", "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, list, domainsCount, "facebook.com", "facebook.com", "facebook.com", "http://facebook.com")

        assertNoCompletion(provider, "wwww")
        assertNoCompletion(provider, "yahoo")
    }

    @Test
    fun `non-empty provider with CUSTOM list returns completion`() {
        val domains = listOf("mozilla.org", "google.com", "facebook.com").into()
        val list = DomainList.CUSTOM
        val domainsCount = domains.size

        val provider = createAndInitProvider(testContext, list) { domains }
        shadowOf(getMainLooper()).idle()

        assertCompletion(provider, list, domainsCount, "m", "m", "mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "moz", "moz", "mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "www", "www", "www.mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "www.face", "www.face", "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, list, domainsCount, "M", "m", "Mozilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "MOZ", "moz", "MOZilla.org", "http://mozilla.org")
        assertCompletion(provider, list, domainsCount, "www.GOO", "www.goo", "www.GOOgle.com", "http://google.com")
        assertCompletion(provider, list, domainsCount, "WWW.GOOGLE.", "www.google.", "WWW.GOOGLE.com", "http://google.com")
        assertCompletion(provider, list, domainsCount, "www.facebook.com", "www.facebook.com", "www.facebook.com", "http://facebook.com")
        assertCompletion(provider, list, domainsCount, "facebook.com", "facebook.com", "facebook.com", "http://facebook.com")

        assertNoCompletion(provider, "wwww")
        assertNoCompletion(provider, "yahoo")
    }
}

private fun assertCompletion(
    provider: DomainAutocompleteProvider,
    domainSource: DomainList,
    sourceSize: Int,
    input: String,
    expectedInput: String,
    completion: String,
    expectedUrl: String,
) {
    val result = provider.getAutocompleteSuggestion(input)!!

    assertTrue("Autocompletion shouldn't be empty", result.text.isNotEmpty())

    assertEquals("Autocompletion input", expectedInput, result.input)
    assertEquals("Autocompletion completion", completion, result.text)
    assertEquals("Autocompletion source list", domainSource.listName, result.source)
    assertEquals("Autocompletion url", expectedUrl, result.url)
    assertEquals("Autocompletion source list size", sourceSize, result.totalItems)
}

private fun assertNoCompletion(provider: DomainAutocompleteProvider, input: String) {
    val result = provider.getAutocompleteSuggestion(input)

    assertNull("Result should be null", result)
}

private fun createAndInitProvider(context: Context, list: DomainList, loader: DomainsLoader): DomainAutocompleteProvider =
    object : BaseDomainAutocompleteProvider(list, loader) {
        override val coroutineContext = super.coroutineContext + Dispatchers.Main
    }.apply { initialize(context) }
