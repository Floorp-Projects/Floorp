/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteResult
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryAutocompleteResult
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.concept.toolbar.Toolbar

/**
 * Feature implementation for connecting a toolbar with a list of autocomplete providers.
 *
 * @property toolbar the [Toolbar] to connect to autocomplete providers.
 * @property engine (optional) instance of a browser [Engine] to issue
 * [Engine.speculativeConnect] calls on successful URL autocompletion.
 * @property shouldAutocomplete (optional) lambda expression that returns true if
 * autocomplete is shown. Otherwise, autocomplete is not shown.
 */
class ToolbarAutocompleteFeature(
    val toolbar: Toolbar,
    val engine: Engine? = null,
    val shouldAutocomplete: () -> Boolean = { true },
) {
    private val historyProviders: MutableList<HistoryStorage> = mutableListOf()
    private val domainProviders: MutableList<DomainAutocompleteProvider> = mutableListOf()

    init {
        toolbar.setAutocompleteListener { query, delegate ->
            if (!shouldAutocomplete()) {
                delegate.noAutocompleteResult(query)
            } else {
                val historyResults = historyProviders.asSequence()
                    .mapNotNull { provider -> provider.getAutocompleteSuggestion(query)?.into() }
                val domainResults = domainProviders.asSequence()
                    .mapNotNull { provider -> provider.getAutocompleteSuggestion(query)?.into() }

                val result = (historyResults + domainResults).firstOrNull()
                if (result != null) {
                    delegate.applyAutocompleteResult(result) {
                        engine?.speculativeConnect(result.url)
                    }
                } else {
                    delegate.noAutocompleteResult(query)
                }
            }
        }
    }

    fun addHistoryStorageProvider(provider: HistoryStorage) {
        historyProviders.add(provider)
    }

    fun addDomainProvider(provider: DomainAutocompleteProvider) {
        domainProviders.add(provider)
    }

    private fun HistoryAutocompleteResult.into() = AutocompleteResult(
        input = input,
        text = text,
        url = url,
        source = source,
        totalItems = totalItems,
    )
    private fun DomainAutocompleteResult.into() = AutocompleteResult(
        input = input,
        text = text,
        url = url,
        source = source,
        totalItems = totalItems,
    )
}
