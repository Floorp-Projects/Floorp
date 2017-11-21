/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.util.Log
import kotlinx.coroutines.experimental.CommonPool
import kotlinx.coroutines.experimental.android.UI
import kotlinx.coroutines.experimental.async
import kotlinx.coroutines.experimental.launch
import org.mozilla.focus.locale.Locales
import org.mozilla.focus.widget.InlineAutocompleteEditText
import java.io.IOException
import java.util.*

class UrlAutoCompleteFilter : InlineAutocompleteEditText.OnFilterListener {
    companion object {
        private val LOG_TAG = "UrlAutoCompleteFilter"
    }

    private val domains = LinkedHashSet<String>()

    override fun onFilter(rawSearchText: String, view: InlineAutocompleteEditText?) {
        if (view == null) {
            return
        }

        // Search terms are all lowercase already, we just need to lowercase the search text
        val searchText = rawSearchText.toLowerCase(Locale.US)

        domains.forEach {
            val wwwDomain = "www." + it
            if (wwwDomain.startsWith(searchText)) {
                view.onAutocomplete(prepareAutocompleteResult(rawSearchText, wwwDomain))
                return
            }

            if (it.startsWith(searchText)) {
                view.onAutocomplete(prepareAutocompleteResult(rawSearchText, it))
                return
            }
        }
    }

    internal fun onDomainsLoaded(domains: Set<String>) {
        this.domains.addAll(domains)
    }

    fun initialize(context: Context) {
        launch(UI) {
            val job = async(CommonPool) {
                loadDomains(context)
            }
            onDomainsLoaded(job.await())
        }
    }

    private suspend fun loadDomains(context: Context): Set<String> {
        val domains = LinkedHashSet<String>()
        val availableLists = getAvailableDomainLists(context)

        // First load the country specific lists following the default locale order
        Locales.getCountriesInDefaultLocaleList()
                .asSequence()
                .filter { availableLists.contains(it) }
                .forEach { loadDomainsForLanguage(context, domains, it) }

        // And then add domains from the global list
        loadDomainsForLanguage(context, domains, "global")

        return domains
    }

    private fun getAvailableDomainLists(context: Context): Set<String> {
        val availableDomains = HashSet<String>()

        val assetManager = context.assets

        try {
            Collections.addAll(availableDomains, *assetManager.list("domains"))
        } catch (e: IOException) {
            Log.w(LOG_TAG, "Could not list domain list directory")
        }

        return availableDomains
    }

    private fun loadDomainsForLanguage(context: Context, domains: MutableSet<String>, country: String) {
        val assetManager = context.assets

        try {
            domains.addAll(
                    assetManager.open("domains/" + country).bufferedReader().readLines())
        } catch (e: IOException) {
            Log.w(LOG_TAG, "Could not load domain list: " + country)
        }
    }

    /**
     * Our autocomplete list is all lower case, however the search text might be mixed case.
     * Our autocomplete EditText code does more string comparison, which fails if the suggestion
     * doesn't exactly match searchText (ie. if casing differs). It's simplest to just build a suggestion
     * that exactly matches the search text - which is what this method is for:
     */
    private fun prepareAutocompleteResult(rawSearchText: String, lowerCaseResult: String) =
            rawSearchText + lowerCaseResult.substring(rawSearchText.length)
}
