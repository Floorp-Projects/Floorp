/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.graphics.drawable.Drawable
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

private const val BOOKMARKS_SUGGESTION_LIMIT = 20

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
 */
class BookmarksStorageSuggestionProvider(
    private val bookmarksStorage: BookmarksStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    private val indicatorIcon: Drawable? = null,
    private val engine: Engine? = null
) : AwesomeBar.SuggestionProvider {

    override val id: String = UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = bookmarksStorage.searchBookmarks(text, BOOKMARKS_SUGGESTION_LIMIT)
            .filter { it.url != null }
            .distinctBy { it.url }
            .sortedBy { it.guid }

        suggestions.firstOrNull()?.url?.let { url -> engine?.speculativeConnect(url) }

        return suggestions.into()
    }

    /**
    * Expects list of BookmarkNode to be specifically of bookmarks (e.g. nodes with a url).
    */
    private suspend fun List<BookmarkNode>.into(): List<AwesomeBar.Suggestion> {
        val iconRequests = this.map { icons?.loadIcon(IconRequest(it.url!!)) }

        return this.zip(iconRequests) { result, icon ->
            AwesomeBar.Suggestion(
                provider = this@BookmarksStorageSuggestionProvider,
                id = result.guid,
                icon = icon?.await()?.bitmap,
                indicatorIcon = indicatorIcon,
                flags = setOf(AwesomeBar.Suggestion.Flag.BOOKMARK),
                title = result.title,
                description = result.url,
                editSuggestion = result.url,
                onSuggestionClicked = { loadUrlUseCase.invoke(result.url!!) }
            )
        }
    }
}
