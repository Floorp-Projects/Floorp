/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.ext.getUrl
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState

/**
 * [Middleware] implementation for handling [MenuAction] and managing the [MenuState] for the menu
 * dialog.
 *
 * @param bookmarksStorage An instance of the [BookmarksStorage] used
 * to query matching bookmarks.
 * @param addBookmarkUseCase The [BookmarksUseCase.AddBookmarksUseCase] for adding the
 * selected tab as a bookmark.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
class MenuDialogMiddleware(
    private val bookmarksStorage: BookmarksStorage,
    private val addBookmarkUseCase: BookmarksUseCase.AddBookmarksUseCase,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<MenuState, MenuAction> {

    override fun invoke(
        context: MiddlewareContext<MenuState, MenuAction>,
        next: (MenuAction) -> Unit,
        action: MenuAction,
    ) {
        when (action) {
            is MenuAction.InitAction -> initialize(context.store)
            is MenuAction.AddBookmark -> addBookmark(context.store)
            else -> Unit
        }

        next(action)
    }

    private fun initialize(
        store: Store<MenuState, MenuAction>,
    ) = scope.launch {
        val url = store.state.browserMenuState?.selectedTab?.content?.url ?: return@launch
        val isBookmarked = bookmarksStorage.getBookmarksWithUrl(url).any { it.url == url }
        store.dispatch(MenuAction.UpdateBookmarked(isBookmarked = isBookmarked))
    }

    private fun addBookmark(
        store: Store<MenuState, MenuAction>,
    ) = scope.launch {
        val browserMenuState = store.state.browserMenuState ?: return@launch

        if (browserMenuState.isBookmarked) {
            return@launch
        }

        val selectedTab = browserMenuState.selectedTab
        val url = selectedTab.getUrl() ?: return@launch

        val isBookmarked = addBookmarkUseCase(
            url = url,
            title = selectedTab.content.title,
        )

        store.dispatch(MenuAction.UpdateBookmarked(isBookmarked = isBookmarked))
    }
}
