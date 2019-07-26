/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_bookmark.view.*
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getColorIntFromAttr
import org.mozilla.fenix.library.LibraryPageView

/**
 * Interface for the Bookmarks view.
 * This interface is implemented by objects that want to respond to user interaction on the bookmarks management UI.
 */
@SuppressWarnings("TooManyFunctions")
interface BookmarkViewInteractor {

    /**
     * Swaps the head of the bookmarks tree, replacing it with a new, updated bookmarks tree.
     *
     * @param node the head node of the new bookmarks tree
     */
    fun change(node: BookmarkNode)

    /**
     * Opens a tab for a bookmark item.
     *
     * @param item the bookmark item to open
     */
    fun open(item: BookmarkNode)

    /**
     * Expands a bookmark folder in the bookmarks tree, providing a view of a different folder elsewhere in the tree.
     *
     * @param folder the bookmark folder to expand
     */
    fun expand(folder: BookmarkNode)

    /**
     * Switches the current bookmark multi-selection mode.
     *
     * @param mode the multi-select mode to switch to
     */
    fun switchMode(mode: BookmarkState.Mode)

    /**
     * Opens up an interface to edit a bookmark node.
     *
     * @param node the bookmark node to edit
     */
    fun edit(node: BookmarkNode)

    /**
     * Selects a bookmark node in multi-selection.
     *
     * @param node the bookmark node to select
     */
    fun select(node: BookmarkNode)

    /**
     * De-selects a bookmark node in multi-selection.
     *
     * @param node the bookmark node to deselect
     */
    fun deselect(node: BookmarkNode)

    /**
     * De-selects all bookmark nodes, clearing the multi-selection mode.
     *
     */
    fun deselectAll()

    /**
     * Copies the URL of a bookmark item to the copy-paste buffer.
     *
     * @param item the bookmark item to copy the URL from
     */
    fun copy(item: BookmarkNode)

    /**
     * Opens the share sheet for a bookmark item.
     *
     * @param item the bookmark item to share
     */
    fun share(item: BookmarkNode)

    /**
     * Opens a bookmark item in a new tab.
     *
     * @param item the bookmark item to open in a new tab
     */
    fun openInNewTab(item: BookmarkNode)

    /**
     * Opens a bookmark item in a private tab.
     *
     * @param item the bookmark item to open in a private tab
     */
    fun openInPrivateTab(item: BookmarkNode)

    /**
     * Deletes a bookmark node.
     *
     * @param node the bookmark node to delete
     */
    fun delete(node: BookmarkNode)

    /**
     * Deletes a set of bookmark nodes.
     *
     * @param nodes the set of bookmark nodes to delete
     */
    fun deleteMulti(nodes: Set<BookmarkNode>)

    /**
     * Handles back presses for the bookmark screen, so navigation up the tree is possible.
     *
     */
    fun backPressed()
}

class BookmarkView(
    private val container: ViewGroup,
    val interactor: BookmarkViewInteractor
) : LibraryPageView(container), LayoutContainer, BackHandler {

    override val containerView: View?
        get() = container

    var mode: BookmarkState.Mode = BookmarkState.Mode.Normal
        private set
    var tree: BookmarkNode? = null
        private set
    private var canGoBack = false

    val view: LinearLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_bookmark, container, true) as LinearLayout

    private val bookmarkAdapter: BookmarkAdapter

    init {
        view.bookmark_list.apply {
            bookmarkAdapter = BookmarkAdapter(view.bookmarks_empty_view, interactor)
            adapter = bookmarkAdapter
        }
    }

    fun update(state: BookmarkState) {
        canGoBack = !(listOf(null, BookmarkRoot.Root.id).contains(state.tree?.guid))
        if (state.tree != tree) {
            tree = state.tree
        }
        if (state.mode != mode) {
            mode = state.mode
            interactor.switchMode(mode)
        }
        when (val modeCopy = state.mode) {
            is BookmarkState.Mode.Normal -> setUIForNormalMode(state.tree)
            is BookmarkState.Mode.Selecting -> setUIForSelectingMode(state.tree, modeCopy)
        }
    }

    override fun onBackPressed(): Boolean {
        return when {
            mode is BookmarkState.Mode.Selecting -> {
                interactor.deselectAll()
                true
            }
            canGoBack -> {
                interactor.backPressed()
                true
            }
            else -> false
        }
    }

    fun getSelected(): Set<BookmarkNode> = bookmarkAdapter.selected

    private fun setUIForSelectingMode(
        root: BookmarkNode?,
        mode: BookmarkState.Mode.Selecting
    ) {
        bookmarkAdapter.updateData(root, mode)
        activity?.title =
            context.getString(R.string.bookmarks_multi_select_title, mode.selectedItems.size)
        setToolbarColors(
            R.color.white_color,
            R.attr.accentHighContrast.getColorIntFromAttr(context)
        )
    }

    private fun setUIForNormalMode(root: BookmarkNode?) {
        bookmarkAdapter.updateData(root, BookmarkState.Mode.Normal)
        setTitle(root)
        setToolbarColors(
            R.attr.primaryText.getColorIntFromAttr(context),
            R.attr.foundation.getColorIntFromAttr(context)
        )
    }

    private fun setTitle(root: BookmarkNode?) {
        activity?.title = when (root?.guid) {
            BookmarkRoot.Mobile.id, null -> context.getString(R.string.library_bookmarks)
            else -> root.title
        }
    }
}
