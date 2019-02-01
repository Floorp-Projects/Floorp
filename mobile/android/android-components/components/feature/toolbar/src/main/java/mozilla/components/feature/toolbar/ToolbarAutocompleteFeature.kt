/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteResult
import mozilla.components.concept.storage.HistoryAutocompleteResult
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.concept.toolbar.Toolbar

/**
 * Feature implementation for connecting a toolbar with a list of autocomplete providers.
 */
class ToolbarAutocompleteFeature(val toolbar: Toolbar) {
    private val historyProviders: MutableList<HistoryStorage> = mutableListOf()
    private val domainProviders: MutableList<DomainAutocompleteProvider> = mutableListOf()
    private val allProviders = { historyProviders.asSequence() + domainProviders }

    init {
        toolbar.setAutocompleteListener { value, delegate ->
            for (provider in allProviders()) {
                val result = provider.autocomplete(value)
                if (result != null) {
                    delegate.applyAutocompleteResult(result)
                    return@setAutocompleteListener
                }
            }
            delegate.noAutocompleteResult(value)
        }
    }

    fun addHistoryStorageProvider(provider: HistoryStorage) {
        historyProviders.add(provider)
    }

    fun addDomainProvider(provider: DomainAutocompleteProvider) {
        domainProviders.add(provider)
    }

    // The rest is a little bit of boilerplate to tie things together.
    // We terminate our dependency chains at the 'feature' level. This is an alternative to creating
    // an "AutocompleteProvider" concept. This pattern allows a storage component, for example, to
    // be standalone and not depend on something like 'concept-domains'.
    // This pattern also helps prevent circular dependency situations from arising over time.

    private fun Any.autocomplete(query: String): AutocompleteResult? {
        if (this is HistoryStorage) {
            return this.getAutocompleteSuggestion(query)?.into()
        } else if (this is DomainAutocompleteProvider) {
            return this.getAutocompleteSuggestion(query)?.into()
        }
        return null
    }

    private fun HistoryAutocompleteResult.into(): AutocompleteResult {
        return AutocompleteResult(
            input = this.input, text = this.text, url = this.url, source = this.source,
            totalItems = this.totalItems
        )
    }

    private fun DomainAutocompleteResult.into(): AutocompleteResult {
        return AutocompleteResult(
            input = this.input, text = this.text, url = this.url, source = this.source,
            totalItems = this.totalItems
        )
    }
}
