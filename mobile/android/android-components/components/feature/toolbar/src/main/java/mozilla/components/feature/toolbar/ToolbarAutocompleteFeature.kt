/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.toolbar.AutocompleteProvider
import mozilla.components.concept.toolbar.Toolbar
import java.util.SortedSet

/**
 * Feature implementation for connecting a toolbar with a list of autocomplete providers.
 *
 * @param toolbar the [Toolbar] to connect to autocomplete providers.
 * @param engine (optional) instance of a browser [Engine] to issue
 * [Engine.speculativeConnect] calls on successful URL autocompletion.
 * @param shouldAutocomplete (optional) lambda expression that returns true if
 * autocomplete is shown. Otherwise, autocomplete is not shown.
 * @param scope (optional) [CoroutineScope] in which to query autocompletion providers.
 */
class ToolbarAutocompleteFeature(
    val toolbar: Toolbar,
    val engine: Engine? = null,
    val shouldAutocomplete: () -> Boolean = { true },
) {
    @VisibleForTesting
    internal var autocompleteProviders: SortedSet<AutocompleteProvider> = sortedSetOf()

    init {
        toolbar.setAutocompleteListener { query, delegate ->
            if (!shouldAutocomplete() || autocompleteProviders.isEmpty() || query.isBlank()) {
                delegate.noAutocompleteResult(query)
            } else {
                val result = autocompleteProviders
                    .firstNotNullOfOrNull { it.getAutocompleteSuggestion(query) }

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

    /**
     * Update the list of providers used for autocompletion results.
     * Changes will take effect the next time user changes their input.
     *
     * @param providers New list of autocomplete providers.
     * The list can be empty in which case autocompletion will be disabled until there is at least one provider.
     * @param refreshAutocomplete Whether to immediately update the autocompletion suggestion
     * based on the new providers.
     */
    @Synchronized
    fun updateAutocompleteProviders(
        providers: List<AutocompleteProvider>,
        refreshAutocomplete: Boolean = true,
    ) {
        autocompleteProviders.clear()
        autocompleteProviders.addAll(providers)
        if (refreshAutocomplete) toolbar.refreshAutocomplete()
    }

    /**
     * Adds the specified provider to the current list of providers.
     *
     * @param provider New [AutocompleteProvider] to add to the current list.
     * If this exact instance already exists it will not be added again.
     * @param refreshAutocomplete Whether to immediately update the autocompletion suggestion
     * based on the new providers.
     *
     * @return `true` if the provider has been added, `false` if the provider already exists.
     */
    @Synchronized
    fun addAutocompleteProvider(
        provider: AutocompleteProvider,
        refreshAutocomplete: Boolean = true,
    ): Boolean {
        return autocompleteProviders.add(provider).also {
            if (refreshAutocomplete) toolbar.refreshAutocomplete()
        }
    }

    /**
     * Remove an autocomplete provider from the current providers list.
     *
     * @param provider [AutocompleteProvider] instance to remove from the current list.
     * If it isn't set already calling this method will have no effect.
     * @param refreshAutocomplete Whether to immediately update the autocompletion suggestion
     * based on the new providers.
     *
     * @return `true` if the provider has been removed, `false` if the provider could not be found.
     */
    @Synchronized
    fun removeAutocompleteProvider(
        provider: AutocompleteProvider,
        refreshAutocomplete: Boolean = true,
    ): Boolean {
        return autocompleteProviders.remove(provider).also {
            if (refreshAutocomplete) toolbar.refreshAutocomplete()
        }
    }
}
