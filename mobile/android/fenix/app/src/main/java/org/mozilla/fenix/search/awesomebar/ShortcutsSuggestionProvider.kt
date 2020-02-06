/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import android.content.Context
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.graphics.drawable.toBitmap
import mozilla.components.browser.search.SearchEngine
import mozilla.components.concept.awesomebar.AwesomeBar
import org.mozilla.fenix.R
import org.mozilla.fenix.components.searchengine.FenixSearchEngineProvider
import java.util.UUID

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides search engine suggestions.
 */
class ShortcutsSuggestionProvider(
    private val searchEngineProvider: FenixSearchEngineProvider,
    private val context: Context,
    private val selectShortcutEngine: (engine: SearchEngine) -> Unit,
    private val selectShortcutEngineSettings: () -> Unit
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override val shouldClearSuggestions: Boolean
        get() = false

    private val settingsIcon by lazy {
        AppCompatResources.getDrawable(context, R.drawable.ic_settings)?.toBitmap()
    }

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        val suggestions = mutableListOf<AwesomeBar.Suggestion>()

        searchEngineProvider.installedSearchEngines(context).list.forEach {
            suggestions.add(
                AwesomeBar.Suggestion(
                    provider = this,
                    id = it.identifier,
                    icon = it.icon,
                    title = it.name,
                    onSuggestionClicked = {
                        selectShortcutEngine(it)
                    })
            )
        }

        suggestions.add(
            AwesomeBar.Suggestion(
                provider = this,
                id = context.getString(R.string.search_shortcuts_engine_settings),
                icon = settingsIcon,
                title = context.getString(R.string.search_shortcuts_engine_settings),
                onSuggestionClicked = {
                    selectShortcutEngineSettings()
                })
        )
        return suggestions
    }
}
