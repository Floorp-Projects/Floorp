/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingMode
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.copyUrl
import org.mozilla.fenix.ext.nav

/**
 * Interactor for the Bookmarks screen.
 * Provides implementations for the BookmarkViewInteractor.
 *
 * @property context The current Android Context
 * @property navController The Android Navigation NavController
 * @property bookmarkStore The BookmarkStore
 * @property sharedViewModel The shared ViewModel used between the Bookmarks screens
 * @property snackbarPresenter A presenter for the FenixSnackBar
 * @property deleteBookmarkNodes A lambda function for deleting bookmark nodes with undo
 */
@SuppressWarnings("TooManyFunctions")
class BookmarkFragmentInteractor(
    private val context: Context,
    private val navController: NavController,
    private val bookmarkStore: BookmarkStore,
    private val sharedViewModel: BookmarksSharedViewModel,
    private val snackbarPresenter: FenixSnackbarPresenter,
    private val deleteBookmarkNodes: (Set<BookmarkNode>, Event) -> Unit
) : BookmarkViewInteractor, SignInInteractor {

    val activity: HomeActivity?
        get() = context.asActivity() as? HomeActivity
    val metrics: MetricController
        get() = context.components.analytics.metrics

    override fun change(node: BookmarkNode) {
        bookmarkStore.dispatch(BookmarkAction.Change(node))
    }

    override fun open(item: BookmarkNode) {
        when (item.type) {
            BookmarkNodeType.ITEM -> openItem(item)
            BookmarkNodeType.FOLDER -> {
                navController.nav(
                    R.id.bookmarkFragment,
                    BookmarkFragmentDirections.actionBookmarkFragmentSelf(item.guid)
                )
            }
            BookmarkNodeType.SEPARATOR -> throw IllegalStateException("Cannot open separators")
        }
    }

    override fun switchMode(mode: BookmarkState.Mode) {
        activity?.invalidateOptionsMenu()
    }

    override fun edit(node: BookmarkNode) {
        navController.nav(
            R.id.bookmarkFragment,
            BookmarkFragmentDirections
                .actionBookmarkFragmentToBookmarkEditFragment(node.guid)
        )
    }

    override fun select(item: BookmarkNode) {
        if (item.inRoots()) {
            snackbarPresenter.present(context.getString(R.string.bookmark_cannot_edit_root))
            return
        }
        bookmarkStore.dispatch(BookmarkAction.Select(item))
    }

    override fun deselect(item: BookmarkNode) {
        bookmarkStore.dispatch(BookmarkAction.Deselect(item))
    }

    override fun deselectAll() {
        bookmarkStore.dispatch(BookmarkAction.DeselectAll)
    }

    override fun copy(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.copyUrl(activity!!)
        snackbarPresenter.present(context.getString(R.string.url_copied))
        metrics.track(Event.CopyBookmark)
    }

    override fun share(item: BookmarkNode) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.apply {
            navController.nav(
                R.id.bookmarkFragment,
                BookmarkFragmentDirections.actionBookmarkFragmentToShareFragment(
                    url = this,
                    title = item.title
                )
            )
            metrics.track(Event.ShareBookmark)
        }
    }

    override fun openInNewTab(item: BookmarkNode) {
        openItem(item, BrowsingMode.Normal)
    }

    override fun openInPrivateTab(item: BookmarkNode) {
        openItem(item, BrowsingMode.Private)
    }

    private fun openItem(item: BookmarkNode, tabMode: BrowsingMode? = null) {
        require(item.type == BookmarkNodeType.ITEM)
        item.url?.let { url ->
            tabMode?.let { activity?.browsingModeManager?.mode = it }
            activity?.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = true,
                from = BrowserDirection.FromBookmarks
            )
            metrics.track(
                when (tabMode) {
                    BrowsingMode.Private -> Event.OpenedBookmarkInPrivateTab
                    BrowsingMode.Normal -> Event.OpenedBookmarkInNewTab
                    null -> Event.OpenedBookmark
                }
            )
        }
    }

    override fun delete(nodes: Set<BookmarkNode>) {
        val eventType = when (nodes.singleOrNull()?.type) {
            BookmarkNodeType.ITEM -> Event.RemoveBookmark
            BookmarkNodeType.FOLDER -> Event.RemoveBookmarkFolder
            BookmarkNodeType.SEPARATOR -> throw IllegalStateException("Cannot delete separators")
            null -> Event.RemoveBookmarks
        }
        deleteBookmarkNodes(nodes, eventType)
    }

    override fun backPressed() {
        navController.popBackStack()
    }

    override fun clickedSignIn() {
        context.components.services.launchPairingSignIn(context, navController)
    }

    override fun signedIn() {
        sharedViewModel.signedIn.postValue(true)
    }

    override fun signedOut() {
        sharedViewModel.signedIn.postValue(false)
    }
}
