/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.graphics.Bitmap
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.facts.emitSearchActionClickedFact
import mozilla.components.feature.search.SearchUseCases

private const val FIXED_ID = "@@@search.action.provider.fixed.id@@"

/**
 * An [AwesomeBar.SuggestionProvider] implementation that returns a suggestion that mirrors the
 * entered text and invokes a search with the given [SearchEngine] if clicked.
 */
class SearchActionProvider(
    private val store: BrowserStore,
    private val searchUseCase: SearchUseCases.SearchUseCase,
    private val icon: Bitmap? = null,
    private val showDescription: Boolean = true,
    private val searchEngine: SearchEngine? = null,
) : AwesomeBar.SuggestionProvider {
    override val id: String = java.util.UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isBlank()) {
            return emptyList()
        }

        val searchEngine = searchEngine ?: store.state.search.selectedOrDefaultSearchEngine
            ?: return emptyList()

        return listOf(
            AwesomeBar.Suggestion(
                provider = this,
                // We always use the same ID for the entered text so that this suggestion gets replaced "in place".
                id = FIXED_ID,
                title = text,
                description = if (showDescription) searchEngine.name else null,
                icon = icon ?: searchEngine.icon,
                score = Int.MAX_VALUE - 1,
                onSuggestionClicked = {
                    searchUseCase.invoke(text)
                    emitSearchActionClickedFact()
                },
            ),
        )
    }
}
