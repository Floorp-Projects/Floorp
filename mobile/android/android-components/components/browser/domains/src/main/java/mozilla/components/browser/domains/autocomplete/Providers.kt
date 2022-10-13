/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains.autocomplete

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.CustomDomains
import mozilla.components.browser.domains.Domain
import mozilla.components.browser.domains.Domains
import mozilla.components.browser.domains.into
import java.util.Locale

enum class DomainList(val listName: String) {
    DEFAULT("default"),
    CUSTOM("custom"),
}

/**
 * Provides autocomplete functionality for domains based on provided list of assets (see [Domains]).
 */
class ShippedDomainsProvider : BaseDomainAutocompleteProvider(DomainList.DEFAULT, Domains.asLoader())

/**
 * Provides autocomplete functionality for domains based on a list managed by [CustomDomains].
 */
class CustomDomainsProvider : BaseDomainAutocompleteProvider(DomainList.CUSTOM, CustomDomains.asLoader())

interface DomainAutocompleteProvider {
    fun getAutocompleteSuggestion(query: String): DomainAutocompleteResult?
}

typealias DomainsLoader = (Context) -> List<Domain>

private fun Domains.asLoader(): DomainsLoader = { context: Context -> load(context).into() }
private fun CustomDomains.asLoader(): DomainsLoader = { context: Context -> load(context).into() }

/**
 * Provides common autocomplete functionality powered by domain lists.
 *
 * @param list source of domains
 * @param domainsLoader provider for all available domains
 */
open class BaseDomainAutocompleteProvider(
    private val list: DomainList,
    private val domainsLoader: DomainsLoader,
) : DomainAutocompleteProvider, CoroutineScope by CoroutineScope(Dispatchers.IO) {

    // We compute 'domains' on the worker thread; make sure it's immediately visible on the UI thread.
    @Volatile
    var domains: List<Domain> = emptyList()

    fun initialize(context: Context) {
        launch {
            domains = async { domainsLoader(context) }.await()
        }
    }

    /**
     * Computes an autocomplete suggestion for the given text, and invokes the
     * provided callback, passing the result.
     *
     * @param query text to be auto completed
     * @return the result of auto-completion, or null if no match is found.
     */
    override fun getAutocompleteSuggestion(query: String): DomainAutocompleteResult? {
        // Search terms are all lowercase already, we just need to lowercase the search text
        val searchText = query.lowercase(Locale.US)

        domains.forEach {
            val wwwDomain = "www.${it.host}"
            if (wwwDomain.startsWith(searchText)) {
                return DomainAutocompleteResult(
                    input = searchText,
                    text = getResultText(query, wwwDomain),
                    url = it.url,
                    source = list.listName,
                    totalItems = domains.size,
                )
            }

            if (it.host.startsWith(searchText)) {
                return DomainAutocompleteResult(
                    input = searchText,
                    text = getResultText(query, it.host),
                    url = it.url,
                    source = list.listName,
                    totalItems = domains.size,
                )
            }
        }

        return null
    }

    /**
     * Our autocomplete list is all lower case, however the search text might
     * be mixed case. Our autocomplete EditText code does more string comparison,
     * which fails if the suggestion doesn't exactly match searchText (ie.
     * if casing differs). It's simplest to just build a suggestion
     * that exactly matches the search text - which is what this method is for:
     */
    private fun getResultText(rawSearchText: String, autocomplete: String) =
        rawSearchText + autocomplete.substring(rawSearchText.length)
}

/**
 * Describes an autocompletion result against a list of domains.
 * @property input Input for which this result is being provided.
 * @property text Result of autocompletion, text to be displayed.
 * @property url Result of autocompletion, full matching url.
 * @property source Name of the autocompletion source.
 * @property totalItems A total number of results also available.
 */
class DomainAutocompleteResult(
    val input: String,
    val text: String,
    val url: String,
    val source: String,
    val totalItems: Int,
)
