/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.BookmarksManagement
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.MetricsUtils

/**
 * Interactor for the Bookmarks screen.
 * Provides implementations for the BookmarkViewInteractor.
 *
 * @param bookmarksController view controller
 */
@SuppressWarnings("TooManyFunctions")
class BookmarkFragmentInteractor(
    private val bookmarksController: BookmarkController,
) : BookmarkViewInteractor {

    override fun onBookmarksChanged(node: BookmarkNode) {
        bookmarksController.handleBookmarkChanged(node)
    }

    override fun onSelectionModeSwitch(mode: BookmarkFragmentState.Mode) {
        bookmarksController.handleSelectionModeSwitch()
    }

    override fun onEditPressed(node: BookmarkNode) {
        bookmarksController.handleBookmarkEdit(node)
        MetricsUtils.recordBookmarkMetrics(
            MetricsUtils.BookmarkAction.EDIT,
            METRIC_SOURCE,
        )
    }

    override fun onAllBookmarksDeselected() {
        bookmarksController.handleAllBookmarksDeselected()
    }

    override fun onSearch() {
        BookmarksManagement.searchIconTapped.record(NoExtras())
        bookmarksController.handleSearch()
    }

    /**
     * Copies the URL of the given BookmarkNode into the copy and paste buffer.
     */
    override fun onCopyPressed(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleCopyUrl(item)
            BookmarksManagement.copied.record(NoExtras())
        }
    }

    override fun onSharePressed(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleBookmarkSharing(item)
            BookmarksManagement.shared.record(NoExtras())
        }
    }

    override fun onOpenInNormalTab(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleOpeningBookmark(item, BrowsingMode.Normal)
            BookmarksManagement.openInNewTab.record(NoExtras())
        }
        MetricsUtils.recordBookmarkMetrics(
            MetricsUtils.BookmarkAction.OPEN,
            METRIC_SOURCE,
        )
    }

    override fun onOpenInPrivateTab(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let {
            bookmarksController.handleOpeningBookmark(item, BrowsingMode.Private)
            BookmarksManagement.openInPrivateTab.record(NoExtras())
        }
        MetricsUtils.recordBookmarkMetrics(
            MetricsUtils.BookmarkAction.OPEN,
            METRIC_SOURCE,
        )
    }

    override fun onOpenAllInNewTabs(folder: BookmarkNode) {
        require(folder.type == BookmarkNodeType.FOLDER)
        bookmarksController.handleOpeningFolderBookmarks(folder, BrowsingMode.Normal)
        MetricsUtils.recordBookmarkMetrics(
            MetricsUtils.BookmarkAction.OPEN,
            METRIC_SOURCE,
        )
    }

    override fun onOpenAllInPrivateTabs(folder: BookmarkNode) {
        require(folder.type == BookmarkNodeType.FOLDER)
        bookmarksController.handleOpeningFolderBookmarks(folder, BrowsingMode.Private)
        MetricsUtils.recordBookmarkMetrics(
            MetricsUtils.BookmarkAction.OPEN,
            METRIC_SOURCE,
        )
    }

    override fun onDelete(nodes: Set<BookmarkNode>) {
        if (nodes.find { it.type == BookmarkNodeType.SEPARATOR } != null) {
            throw IllegalStateException("Cannot delete separators")
        }
        val eventType = when (nodes.singleOrNull()?.type) {
            BookmarkNodeType.ITEM,
            BookmarkNodeType.SEPARATOR,
            -> BookmarkRemoveType.SINGLE
            BookmarkNodeType.FOLDER -> BookmarkRemoveType.FOLDER
            null -> BookmarkRemoveType.MULTIPLE
        }
        MetricsUtils.recordBookmarkMetrics(
            MetricsUtils.BookmarkAction.DELETE,
            METRIC_SOURCE,
        )
        if (eventType == BookmarkRemoveType.FOLDER) {
            bookmarksController.handleBookmarkFolderDeletion(nodes)
        } else {
            bookmarksController.handleBookmarkDeletion(nodes, eventType)
        }
    }

    override fun onBackPressed() {
        bookmarksController.handleBackPressed()
    }

    override fun open(item: BookmarkNode) {
        when (item.type) {
            BookmarkNodeType.ITEM -> {
                bookmarksController.handleBookmarkTapped(item)
                BookmarksManagement.open.record(NoExtras())
            }
            BookmarkNodeType.FOLDER -> bookmarksController.handleBookmarkExpand(item)
            BookmarkNodeType.SEPARATOR -> throw IllegalStateException("Cannot open separators")
        }
    }

    override fun select(item: BookmarkNode) {
        bookmarksController.handleBookmarkSelected(item)
    }

    override fun deselect(item: BookmarkNode) {
        bookmarksController.handleBookmarkDeselected(item)
    }

    override fun onRequestSync() {
        bookmarksController.handleRequestSync()
    }

    companion object {
        const val METRIC_SOURCE = "bookmark_panel"
    }
}
