/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipData
import android.content.ClipboardManager
import android.content.res.Resources
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.fxa.sync.SyncReason
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav

/**
 * [BookmarkFragment] controller.
 * Delegated by View Interactors, handles container business logic and operates changes on it.
 */
@Suppress("TooManyFunctions")
interface BookmarkController {
    fun handleBookmarkChanged(item: BookmarkNode)
    fun handleBookmarkTapped(item: BookmarkNode)
    fun handleBookmarkExpand(folder: BookmarkNode)
    fun handleSelectionModeSwitch()
    fun handleBookmarkEdit(node: BookmarkNode)
    fun handleBookmarkSelected(node: BookmarkNode)
    fun handleBookmarkDeselected(node: BookmarkNode)
    fun handleAllBookmarksDeselected()
    fun handleCopyUrl(item: BookmarkNode)
    fun handleBookmarkSharing(item: BookmarkNode)
    fun handleOpeningBookmark(item: BookmarkNode, mode: BrowsingMode)
    fun handleBookmarkDeletion(nodes: Set<BookmarkNode>, eventType: Event)
    fun handleBookmarkFolderDeletion(nodes: Set<BookmarkNode>)
    fun handleRequestSync()
    fun handleBackPressed()
}

@Suppress("TooManyFunctions")
class DefaultBookmarkController(
    private val activity: HomeActivity,
    private val navController: NavController,
    private val clipboardManager: ClipboardManager?,
    private val scope: CoroutineScope,
    private val store: BookmarkFragmentStore,
    private val sharedViewModel: BookmarksSharedViewModel,
    private val tabsUseCases: TabsUseCases?,
    private val loadBookmarkNode: suspend (String) -> BookmarkNode?,
    private val showSnackbar: (String) -> Unit,
    private val deleteBookmarkNodes: (Set<BookmarkNode>, Event) -> Unit,
    private val deleteBookmarkFolder: (Set<BookmarkNode>) -> Unit,
    private val invokePendingDeletion: () -> Unit,
    private val showTabTray: () -> Unit
) : BookmarkController {

    private val resources: Resources = activity.resources

    override fun handleBookmarkChanged(item: BookmarkNode) {
        sharedViewModel.selectedFolder = item
        store.dispatch(BookmarkFragmentAction.Change(item))
    }

    override fun handleBookmarkTapped(item: BookmarkNode) {
        openInNewTabAndShow(item.url!!, true, BrowserDirection.FromBookmarks, activity.browsingModeManager.mode)
    }

    override fun handleBookmarkExpand(folder: BookmarkNode) {
        handleAllBookmarksDeselected()
        invokePendingDeletion.invoke()
        scope.launch {
            val node = loadBookmarkNode.invoke(folder.guid) ?: return@launch
            sharedViewModel.selectedFolder = node
            store.dispatch(BookmarkFragmentAction.Change(node))
        }
    }

    override fun handleSelectionModeSwitch() {
        activity.invalidateOptionsMenu()
    }

    override fun handleBookmarkEdit(node: BookmarkNode) {
        navigateToGivenDirection(BookmarkFragmentDirections.actionBookmarkFragmentToBookmarkEditFragment(node.guid))
    }

    override fun handleBookmarkSelected(node: BookmarkNode) {
        if (store.state.mode is BookmarkFragmentState.Mode.Syncing) {
            return
        }

        if (node.inRoots()) {
            showSnackbar(resources.getString(R.string.bookmark_cannot_edit_root))
        } else {
            store.dispatch(BookmarkFragmentAction.Select(node))
        }
    }

    override fun handleBookmarkDeselected(node: BookmarkNode) {
        store.dispatch(BookmarkFragmentAction.Deselect(node))
    }

    override fun handleAllBookmarksDeselected() {
        store.dispatch(BookmarkFragmentAction.DeselectAll)
    }

    override fun handleCopyUrl(item: BookmarkNode) {
        val urlClipData = ClipData.newPlainText(item.url, item.url)
        clipboardManager?.setPrimaryClip(urlClipData)
        showSnackbar(resources.getString(R.string.url_copied))
    }

    override fun handleBookmarkSharing(item: BookmarkNode) {
        navigateToGivenDirection(
            BookmarkFragmentDirections.actionGlobalShareFragment(
                data = arrayOf(ShareData(url = item.url, title = item.title))
            )
        )
    }

    override fun handleOpeningBookmark(item: BookmarkNode, mode: BrowsingMode) {
        openInNewTab(item.url!!, mode)
        showTabTray()
    }

    override fun handleBookmarkDeletion(nodes: Set<BookmarkNode>, eventType: Event) {
        deleteBookmarkNodes(nodes, eventType)
    }

    override fun handleBookmarkFolderDeletion(nodes: Set<BookmarkNode>) {
        deleteBookmarkFolder(nodes)
    }

    override fun handleRequestSync() {
        scope.launch {
            store.dispatch(BookmarkFragmentAction.StartSync)
            invokePendingDeletion()
            activity.components.backgroundServices.accountManager.syncNow(SyncReason.User)
            // The current bookmark node we are viewing may be made invalid after syncing so we
            // check if the current node is valid and if it isn't we find the nearest valid ancestor
            // and open it
            val validAncestorGuid = store.state.guidBackstack.findLast { guid ->
                activity.bookmarkStorage.getBookmark(guid) != null
            } ?: BookmarkRoot.Mobile.id
            val node = activity.bookmarkStorage.getBookmark(validAncestorGuid)!!
            handleBookmarkExpand(node)
            store.dispatch(BookmarkFragmentAction.FinishSync)
        }
    }

    override fun handleBackPressed() {
        invokePendingDeletion.invoke()
        scope.launch {
            val parentGuid = store.state.guidBackstack.findLast { guid ->
                store.state.tree?.guid != guid && activity.bookmarkStorage.getBookmark(guid) != null
            }
            if (parentGuid == null) {
                navController.popBackStack()
            } else {
                val parent = activity.bookmarkStorage.getBookmark(parentGuid)!!
                handleBookmarkExpand(parent)
            }
        }
    }

    private fun openInNewTabAndShow(
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection,
        mode: BrowsingMode
    ) {
        invokePendingDeletion.invoke()
        with(activity) {
            browsingModeManager.mode = mode
            openToBrowserAndLoad(searchTermOrURL, newTab, from)
        }
    }

    private fun openInNewTab(
        url: String,
        mode: BrowsingMode
    ) {
        invokePendingDeletion.invoke()
        activity.browsingModeManager.mode = BrowsingMode.fromBoolean(mode == BrowsingMode.Private)
        tabsUseCases?.addTab?.invoke(url, private = (mode == BrowsingMode.Private))
    }

    private fun navigateToGivenDirection(directions: NavDirections) {
        invokePendingDeletion.invoke()
        navController.nav(R.id.bookmarkFragment, directions)
    }
}
