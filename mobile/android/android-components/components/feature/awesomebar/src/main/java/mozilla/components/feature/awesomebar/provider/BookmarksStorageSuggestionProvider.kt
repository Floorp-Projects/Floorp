/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.awesomebar.internal.loadLambda
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

private const val BOOKMARKS_SUGGESTION_LIMIT = 20

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on the bookmarks
 * stored in the [BookmarksStorage].
 */
class BookmarksStorageSuggestionProvider(
    private val bookmarksStorage: BookmarksStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null
) : AwesomeBar.SuggestionProvider {

    override val id: String = UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = bookmarksStorage.searchBookmarks(text, BOOKMARKS_SUGGESTION_LIMIT)
        return suggestions.filter { it.url != null }.distinctBy { it.url }.sortedBy { it.guid }
            .into()
    }

    /**
    * Expects list of BookmarkNode to be specifically of bookmarks (e.g. nodes with a url).
    */
    private fun List<BookmarkNode>.into(): List<AwesomeBar.Suggestion> {
        return this.map {
            AwesomeBar.Suggestion(
                provider = this@BookmarksStorageSuggestionProvider,
                id = it.guid,
                icon = icons.loadLambda(it.url!!),
                title = it.title,
                description = it.url,
                onSuggestionClicked = { loadUrlUseCase.invoke(it.url!!) }
            )
        }
    }
}
