/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.graphics.drawable.Drawable
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.awesomebar.facts.emitBookmarkSuggestionClickedFact
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

/**
 * Return up to 20 bookmarks suggestions by default.
 */
@VisibleForTesting
internal const val BOOKMARKS_SUGGESTION_LIMIT = 20

/**
 * Default suggestions limit multiplier when needing to filter results by an external url filter.
 */
@VisibleForTesting
internal const val BOOKMARKS_RESULTS_TO_FILTER_SCALE_FACTOR = 10

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on the bookmarks
 * stored in the [BookmarksStorage].
 *
 * @property bookmarksStorage and instance of the [BookmarksStorage] used
 * to query matching bookmarks.
 * @property loadUrlUseCase the use case invoked to load the url when the
 * user clicks on the suggestion.
 * @property icons optional instance of [BrowserIcons] to load fav icons
 * for bookmarked URLs.
 * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * highest scored suggestion URL.
 * @param showEditSuggestion optional parameter to specify if the suggestion should show the edit button
 * @param suggestionsHeader optional parameter to specify if the suggestion should have a header
 * @param resultsUriFilter Optional predicate to filter matching suggestions by URL.
 */
class BookmarksStorageSuggestionProvider(
    @get:VisibleForTesting internal val bookmarksStorage: BookmarksStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    private val indicatorIcon: Drawable? = null,
    private val engine: Engine? = null,
    @get:VisibleForTesting val showEditSuggestion: Boolean = true,
    private val suggestionsHeader: String? = null,
    @get:VisibleForTesting val resultsUriFilter: ((Uri) -> Boolean)? = null,
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override fun groupTitle(): String? {
        return suggestionsHeader
    }

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        bookmarksStorage.cancelReads(text)

        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = when (resultsUriFilter) {
            null -> getBookmarksSuggestions(text)
            else -> getFilteredBookmarksSuggestions(text, resultsUriFilter)
        }

        suggestions.firstOrNull()?.url?.let { url -> engine?.speculativeConnect(url) }

        return suggestions.into()
    }

    /**
     * Get up to [BOOKMARKS_SUGGESTION_LIMIT] bookmarks matching [query].
     *
     * @param query String to filter bookmarks' title or URL by.
     */
    private suspend fun getBookmarksSuggestions(query: String) = bookmarksStorage
        .searchBookmarks(query, BOOKMARKS_SUGGESTION_LIMIT)
        .filter { it.url != null }
        .distinctBy { it.url }
        .sortedBy { it.guid }

    /**
     * Get up to [BOOKMARKS_SUGGESTION_LIMIT] bookmarks matching [query] and [filter].
     *
     * @param query String to filter bookmarks' title or URL by.
     * @param filter Predicate to filter the URLs of the bookmarks that match the [query].
     */
    private suspend fun getFilteredBookmarksSuggestions(query: String, filter: (Uri) -> Boolean) = bookmarksStorage
        .searchBookmarks(query, BOOKMARKS_SUGGESTION_LIMIT * BOOKMARKS_RESULTS_TO_FILTER_SCALE_FACTOR)
        .filter {
            it.url?.toUri()?.let(filter) ?: true
        }
        .distinctBy { it.url }
        .sortedBy { it.guid }
        .take(BOOKMARKS_SUGGESTION_LIMIT)

    /**
     * Expects list of BookmarkNode to be specifically of bookmarks (e.g. nodes with a url).
     */
    private suspend fun List<BookmarkNode>.into(): List<AwesomeBar.Suggestion> {
        val iconRequests = this.map { icons?.loadIcon(IconRequest(url = it.url!!, waitOnNetworkLoad = false)) }

        return this.zip(iconRequests) { result, icon ->
            AwesomeBar.Suggestion(
                provider = this@BookmarksStorageSuggestionProvider,
                id = result.guid,
                icon = icon?.await()?.bitmap,
                indicatorIcon = indicatorIcon,
                flags = setOf(AwesomeBar.Suggestion.Flag.BOOKMARK),
                title = result.title,
                description = result.url,
                editSuggestion = if (showEditSuggestion) result.url else null,
                onSuggestionClicked = {
                    val flags = LoadUrlFlags.select(LoadUrlFlags.ALLOW_JAVASCRIPT_URL)
                    loadUrlUseCase.invoke(result.url!!, flags = flags)
                    emitBookmarkSuggestionClickedFact()
                },
            )
        }
    }
}
