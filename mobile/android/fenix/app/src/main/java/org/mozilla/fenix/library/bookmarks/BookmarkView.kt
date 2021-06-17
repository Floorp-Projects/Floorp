/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.navigation.NavController
import kotlinx.android.synthetic.main.component_bookmark.view.*
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibraryPageView
import org.mozilla.fenix.selection.SelectionInteractor

/**
 * Interface for the Bookmarks view.
 * This interface is implemented by objects that want to respond to user interaction on the bookmarks management UI.
 */
@SuppressWarnings("TooManyFunctions")
interface BookmarkViewInteractor : SelectionInteractor<BookmarkNode> {

    /**
     * Swaps the head of the bookmarks tree, replacing it with a new, updated bookmarks tree.
     *
     * @param node the head node of the new bookmarks tree
     */
    fun onBookmarksChanged(node: BookmarkNode)

    /**
     * Switches the current bookmark multi-selection mode.
     *
     * @param mode the multi-select mode to switch to
     */
    fun onSelectionModeSwitch(mode: BookmarkFragmentState.Mode)

    /**
     * Opens up an interface to edit a bookmark node.
     *
     * @param node the bookmark node to edit
     */
    fun onEditPressed(node: BookmarkNode)

    /**
     * De-selects all bookmark nodes, clearing the multi-selection mode.
     *
     */
    fun onAllBookmarksDeselected()

    /**
     * Copies the URL of a bookmark item to the copy-paste buffer.
     *
     * @param item the bookmark item to copy the URL from
     */
    fun onCopyPressed(item: BookmarkNode)

    /**
     * Opens the share sheet for a bookmark item.
     *
     * @param item the bookmark item to share
     */
    fun onSharePressed(item: BookmarkNode)

    /**
     * Opens a bookmark item in a new tab.
     *
     * @param item the bookmark item to open in a new tab
     */
    fun onOpenInNormalTab(item: BookmarkNode)

    /**
     * Opens a bookmark item in a private tab.
     *
     * @param item the bookmark item to open in a private tab
     */
    fun onOpenInPrivateTab(item: BookmarkNode)

    /**
     * Deletes a set of bookmark nodes.
     *
     * @param nodes the bookmark nodes to delete
     */
    fun onDelete(nodes: Set<BookmarkNode>)

    /**
     * Handles back presses for the bookmark screen, so navigation up the tree is possible.
     *
     */
    fun onBackPressed()

    /**
     * Handles user requested sync of bookmarks.
     *
     */
    fun onRequestSync()
}

class BookmarkView(
    container: ViewGroup,
    val interactor: BookmarkViewInteractor,
    private val navController: NavController
) : LibraryPageView(container), UserInteractionHandler {

    val view: View = LayoutInflater.from(container.context)
        .inflate(R.layout.component_bookmark, container, true)

    private var mode: BookmarkFragmentState.Mode = BookmarkFragmentState.Mode.Normal()
    private var tree: BookmarkNode? = null

    private val bookmarkAdapter = BookmarkAdapter(view.bookmarks_empty_view, interactor)

    init {
        view.bookmark_list.apply {
            adapter = bookmarkAdapter
        }
        view.bookmark_folders_sign_in.setOnClickListener {
            navController.navigate(NavGraphDirections.actionGlobalTurnOnSync())
        }
        view.swipe_refresh.setOnRefreshListener {
            interactor.onRequestSync()
        }
    }

    fun update(state: BookmarkFragmentState) {
        tree = state.tree
        if (state.mode != mode) {
            mode = state.mode
            if (mode is BookmarkFragmentState.Mode.Normal || mode is BookmarkFragmentState.Mode.Selecting) {
                interactor.onSelectionModeSwitch(mode)
            }
        }

        bookmarkAdapter.updateData(state.tree, mode)

        when (mode) {
            is BookmarkFragmentState.Mode.Normal -> {
                setUiForNormalMode(state.tree)
            }
            is BookmarkFragmentState.Mode.Selecting -> {
                setUiForSelectingMode(
                    context.getString(
                        R.string.bookmarks_multi_select_title,
                        mode.selectedItems.size
                    )
                )
            }
        }
        view.bookmarks_progress_bar.isVisible = state.isLoading
        view.swipe_refresh.isEnabled =
            state.mode is BookmarkFragmentState.Mode.Normal || state.mode is BookmarkFragmentState.Mode.Syncing
        view.swipe_refresh.isRefreshing = state.mode is BookmarkFragmentState.Mode.Syncing
    }

    override fun onBackPressed(): Boolean {
        return when (mode) {
            is BookmarkFragmentState.Mode.Selecting -> {
                interactor.onAllBookmarksDeselected()
                true
            }
            else -> {
                interactor.onBackPressed()
                true
            }
        }
    }

    private fun setUiForNormalMode(root: BookmarkNode?) {
        super.setUiForNormalMode(
            if (BookmarkRoot.Mobile.id == root?.guid) context.getString(R.string.library_bookmarks) else root?.title
        )
    }

    /**
     * Returns true if [root] matches the bookmark root ID.
     */
    private fun BookmarkRoot.matches(root: BookmarkNode?): Boolean {
        return root == null || id == root.guid
    }
}
