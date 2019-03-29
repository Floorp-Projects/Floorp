package org.mozilla.fenix.search.awesomebar

import android.content.Context
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.ktx.android.graphics.drawable.toBitmap
import org.mozilla.fenix.R
import java.util.UUID

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides search engine suggestions.
 */
class ShortcutsSuggestionProvider(
    private val searchEngineManager: SearchEngineManager,
    private val context: Context,
    private val selectShortcutEngine: (engine: SearchEngine) -> Unit,
    private val selectShortcutEngineSettings: () -> Unit
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override val shouldClearSuggestions: Boolean
        get() = false

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        val suggestions = mutableListOf<AwesomeBar.Suggestion>()

        searchEngineManager.getSearchEngines(context).forEach {
            suggestions.add(
                AwesomeBar.Suggestion(
                    provider = this,
                    id = id,
                    icon = { _, _ ->
                        it.icon
                    },
                    title = it.name,
                    onSuggestionClicked = {
                        selectShortcutEngine(it)
                    })
            )
        }

        suggestions.add(
            AwesomeBar.Suggestion(
                provider = this,
                id = id,
                icon = { _, _ ->
                    context.getDrawable(R.drawable.ic_settings)?.toBitmap()
                },
                title = context.getString(R.string.search_shortcuts_engine_settings),
                onSuggestionClicked = {
                    selectShortcutEngineSettings()
                })
        )
        return suggestions
    }
}
