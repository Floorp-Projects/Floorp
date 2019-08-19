/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.BrowsingMode
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.lib.Do

/**
 * Interactor for the Bookmarks screen.
 * Provides implementations for the BookmarkViewInteractor.
 *
 * @property bookmarkStore bookmarks state
 * @property viewModel view state
 * @property bookmarksController view controller
 * @property metrics telemetry controller
 */
@SuppressWarnings("TooManyFunctions")
class BookmarkFragmentInteractor(
    private val bookmarkStore: BookmarkStore,
    private val viewModel: BookmarksSharedViewModel,
    private val bookmarksController: BookmarkController,
    private val metrics: MetricController
) : BookmarkViewInteractor, SignInInteractor {

    override fun onBookmarksChanged(node: BookmarkNode) {
        bookmarkStore.dispatch(BookmarkAction.Change(node))
    }

    override fun onSelectionModeSwitch(mode: BookmarkState.Mode) {
        bookmarksController.handleSelectionModeSwitch()
    }

    override fun onEditPressed(node: BookmarkNode) {
        bookmarksController.handleBookmarkEdit(node)
    }

    override fun onAllBookmarksDeselected() {
        bookmarkStore.dispatch(BookmarkAction.DeselectAll)
    }

    override fun onCopyPressed(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleCopyUrl(item)
            metrics.track(Event.CopyBookmark)
        }
    }

    override fun onSharePressed(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleBookmarkSharing(item)
            metrics.track(Event.ShareBookmark)
        }
    }

    override fun onOpenInNormalTab(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleOpeningBookmark(item, BrowsingMode.Normal)
            metrics.track(Event.OpenedBookmarkInNewTab)
        }
    }

    override fun onOpenInPrivateTab(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleOpeningBookmark(item, BrowsingMode.Private)
            metrics.track(Event.OpenedBookmarkInPrivateTab)
        }
    }

    override fun onDelete(nodes: Set<BookmarkNode>) {
        if (nodes.find { it.type == BookmarkNodeType.SEPARATOR } != null) {
            throw IllegalStateException("Cannot delete separators")
        }
        val eventType = when (nodes.singleOrNull()?.type) {
            BookmarkNodeType.ITEM,
            BookmarkNodeType.SEPARATOR -> Event.RemoveBookmark
            BookmarkNodeType.FOLDER -> Event.RemoveBookmarkFolder
            null -> Event.RemoveBookmarks
        }
        bookmarksController.handleBookmarkDeletion(nodes, eventType)
    }

    override fun onBackPressed() {
        bookmarksController.handleBackPressed()
    }

    override fun onSignInPressed() {
        bookmarksController.handleSigningIn()
    }

    override fun onSignedIn() {
        viewModel.signedIn.postValue(true)
    }

    override fun onSignedOut() {
        viewModel.signedIn.postValue(false)
    }

    override fun open(item: BookmarkNode) {
        Do exhaustive when (item.type) {
            BookmarkNodeType.ITEM -> {
                bookmarksController.handleBookmarkTapped(item)
                metrics.track(Event.OpenedBookmark)
            }
            BookmarkNodeType.FOLDER -> bookmarksController.handleBookmarkExpand(item)
            BookmarkNodeType.SEPARATOR -> throw IllegalStateException("Cannot open separators")
        }
    }

    override fun select(item: BookmarkNode) {
        when (item.inRoots()) {
            true -> bookmarksController.handleBookmarkSelected(item)
            false -> bookmarkStore.dispatch(BookmarkAction.Select(item))
        }
    }

    override fun deselect(item: BookmarkNode) {
        bookmarkStore.dispatch(BookmarkAction.Deselect(item))
    }
}
