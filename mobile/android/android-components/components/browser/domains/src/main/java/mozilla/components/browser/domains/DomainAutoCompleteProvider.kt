/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import java.util.Locale

/**
 * Provides autocomplete functionality for domains, based on a provided list
 * of assets (see [Domains]) and/or a custom domain list managed by
 * [CustomDomains].
 */
// FIXME delete this https://github.com/mozilla-mobile/android-components/issues/1358
@Deprecated(
    "Use `ShippedDomainsProvider` or `CustomDomainsProvider`",
    ReplaceWith(
        "ShippedDomainsProvider()/CustomDomainsProvider()",
        "mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider",
        "mozilla.components.browser.domains.autocomplete.CustomDomainsProvider",
    ),
)
class DomainAutoCompleteProvider {

    object AutocompleteSource {
        const val DEFAULT_LIST = "default"
        const val CUSTOM_LIST = "custom"
    }

    /**
     * Represents a result of auto-completion.
     *
     * @property text the result text starting with the raw search text as passed
     * to [autocomplete] followed by the completion text (e.g. moz => mozilla.org)
     * @property url the complete url (containing the protocol) as provided
     * when the domain was saved. (e.g. https://mozilla.org)
     * @property source the source identifier of the autocomplete source
     * @property size total number of available autocomplete domains
     * in this source
     */
    data class Result(val text: String, val url: String, val source: String, val size: Int)

    // We compute these on worker threads; make sure results are immediately visible on the UI thread.
    @Volatile
    internal var customDomains = emptyList<Domain>()

    @Volatile
    internal var shippedDomains = emptyList<Domain>()
    private var useCustomDomains = false
    private var useShippedDomains = true

    /**
     * Computes an autocomplete suggestion for the given text, and invokes the
     * provided callback, passing the result.
     *
     * @param rawText text to be auto completed
     * @return the result of auto-completion. If no match is found an empty
     * result object is returned.
     */
    @Suppress("ReturnCount")
    fun autocomplete(rawText: String): Result {
        if (useCustomDomains) {
            val result = tryToAutocomplete(rawText, customDomains, AutocompleteSource.CUSTOM_LIST)
            if (result != null) {
                return result
            }
        }

        if (useShippedDomains) {
            val result = tryToAutocomplete(rawText, shippedDomains, AutocompleteSource.DEFAULT_LIST)
            if (result != null) {
                return result
            }
        }

        return Result("", "", "", 0)
    }

    /**
     * Initializes this provider instance by making sure the shipped and/or custom
     * domains are loaded.
     *
     * @param context the application context
     * @param useShippedDomains true (default) if the domains provided by this
     * module should be used, otherwise false.
     * @param useCustomDomains true if the custom domains provided by
     * [CustomDomains] should be used, otherwise false (default).
     * @param loadDomainsFromDisk true (default) if domains should be loaded,
     * otherwise false. This parameter is for testing purposes only.
     */
    fun initialize(
        context: Context,
        useShippedDomains: Boolean = true,
        useCustomDomains: Boolean = false,
        loadDomainsFromDisk: Boolean = true,
    ) {
        this.useCustomDomains = useCustomDomains
        this.useShippedDomains = useShippedDomains

        if (!loadDomainsFromDisk) {
            return
        }

        if (!useCustomDomains && !useShippedDomains) {
            return
        }

        CoroutineScope(Dispatchers.IO).launch {
            if (useCustomDomains) {
                customDomains = async { CustomDomains.load(context).into() }.await()
            }
            if (useShippedDomains) {
                shippedDomains = async { Domains.load(context).into() }.await()
            }
        }
    }

    @Suppress("ReturnCount")
    private fun tryToAutocomplete(rawText: String, domains: List<Domain>, source: String): Result? {
        // Search terms are all lowercase already, we just need to lowercase the search text
        val searchText = rawText.lowercase(Locale.US)

        domains.forEach {
            val wwwDomain = "www.${it.host}"
            if (wwwDomain.startsWith(searchText)) {
                return Result(getResultText(rawText, wwwDomain), it.url, source, domains.size)
            }

            if (it.host.startsWith(searchText)) {
                return Result(getResultText(rawText, it.host), it.url, source, domains.size)
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
