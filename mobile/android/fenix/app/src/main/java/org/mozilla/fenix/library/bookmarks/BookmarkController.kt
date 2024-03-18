/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipData
import android.content.ClipboardManager
import android.content.res.Resources
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.sync.SyncReason
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe

@VisibleForTesting
internal const val WARN_OPEN_ALL_SIZE = 15

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
    fun handleOpeningFolderBookmarks(folder: BookmarkNode, mode: BrowsingMode)

    /**
     * Handle bookmark nodes deletion
     * @param nodes The set of nodes to be deleted.
     * @param removeType Type of removal.
     */
    fun handleBookmarkDeletion(nodes: Set<BookmarkNode>, removeType: BookmarkRemoveType)
    fun handleBookmarkFolderDeletion(nodes: Set<BookmarkNode>)
    fun handleRequestSync()
    fun handleBackPressed()
    fun handleSearch()
}

/**
 * Type of bookmark nodes deleted.
 */
enum class BookmarkRemoveType {
    SINGLE, MULTIPLE, FOLDER
}

@Suppress("TooManyFunctions", "LongParameterList")
class DefaultBookmarkController(
    private val activity: HomeActivity,
    private val navController: NavController,
    private val clipboardManager: ClipboardManager?,
    private val scope: CoroutineScope,
    private val store: BookmarkFragmentStore,
    private val sharedViewModel: BookmarksSharedViewModel,
    private val tabsUseCases: TabsUseCases?,
    private val loadBookmarkNode: suspend (String, Boolean) -> BookmarkNode?,
    private val showSnackbar: (String) -> Unit,
    private val deleteBookmarkNodes: (Set<BookmarkNode>, BookmarkRemoveType) -> Unit,
    private val deleteBookmarkFolder: (Set<BookmarkNode>) -> Unit,
    private val showTabTray: (Boolean) -> Unit,
    private val warnLargeOpenAll: (Int, () -> Unit) -> Unit,
) : BookmarkController {

    private val resources: Resources = activity.resources

    override fun handleBookmarkChanged(item: BookmarkNode) {
        sharedViewModel.selectedFolder = item
        store.dispatch(BookmarkFragmentAction.Change(item))
    }

    override fun handleBookmarkTapped(item: BookmarkNode) {
        val fromHomeFragment =
            navController.previousBackStackEntry?.destination?.id == R.id.homeFragment
        val isPrivate = activity.browsingModeManager.mode == BrowsingMode.Private
        val flags = EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.ALLOW_JAVASCRIPT_URL)
        openInNewTabAndShow(
            item.url!!,
            isPrivate || fromHomeFragment,
            BrowserDirection.FromBookmarks,
            activity.browsingModeManager.mode,
            flags,
        )
    }

    override fun handleBookmarkExpand(folder: BookmarkNode) {
        handleAllBookmarksDeselected()
        scope.launch {
            val node = loadBookmarkNode.invoke(folder.guid, false) ?: return@launch
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
                data = arrayOf(ShareData(url = item.url, title = item.title)),
            ),
        )
    }

    override fun handleOpeningBookmark(item: BookmarkNode, mode: BrowsingMode) {
        openInNewTab(item.url!!, mode)
        showTabTray(mode.isPrivate)
    }

    private fun extractURLsFromTree(node: BookmarkNode): MutableList<String> {
        val urls = mutableListOf<String>()

        when (node.type) {
            BookmarkNodeType.FOLDER -> {
                node.children?.forEach {
                    urls.addAll(extractURLsFromTree(it))
                }
            }
            BookmarkNodeType.ITEM -> {
                node.url?.let { urls.add(it) }
            }
            BookmarkNodeType.SEPARATOR -> {}
        }

        return urls
    }

    override fun handleOpeningFolderBookmarks(folder: BookmarkNode, mode: BrowsingMode) {
        scope.launch {
            val tree = loadBookmarkNode.invoke(folder.guid, true) ?: return@launch
            val urls = extractURLsFromTree(tree)

            val openAll = { load: Boolean ->
                for (url in urls) {
                    tabsUseCases?.addTab?.invoke(
                        url,
                        private = (mode == BrowsingMode.Private),
                        startLoading = load,
                    )
                }
                activity.browsingModeManager.mode =
                    BrowsingMode.fromBoolean(mode == BrowsingMode.Private)
                showTabTray(mode.isPrivate)
            }

            // Warn user if more than maximum number of bookmarks are being opened
            if (urls.size >= WARN_OPEN_ALL_SIZE) {
                warnLargeOpenAll(urls.size) {
                    openAll(false)
                }
            } else {
                openAll(true)
            }
        }
    }

    override fun handleBookmarkDeletion(nodes: Set<BookmarkNode>, removeType: BookmarkRemoveType) {
        deleteBookmarkNodes(nodes, removeType)
    }

    override fun handleBookmarkFolderDeletion(nodes: Set<BookmarkNode>) {
        deleteBookmarkFolder(nodes)
    }

    override fun handleRequestSync() {
        scope.launch {
            store.dispatch(BookmarkFragmentAction.StartSync)
            activity.components.backgroundServices.accountManager.syncNow(
                reason = SyncReason.User,
                debounce = true,
                customEngineSubset = listOf(SyncEngine.Bookmarks),
            )
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

    override fun handleSearch() {
        navController.navigateSafe(
            R.id.bookmarkFragment,
            BookmarkFragmentDirections.actionGlobalSearchDialog(sessionId = null),
        )
    }

    private fun openInNewTabAndShow(
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection,
        mode: BrowsingMode,
        flags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
    ) {
        with(activity) {
            browsingModeManager.mode = mode
            openToBrowserAndLoad(searchTermOrURL, newTab, from, flags = flags)
        }
    }

    private fun openInNewTab(
        url: String,
        mode: BrowsingMode,
    ) {
        activity.browsingModeManager.mode = BrowsingMode.fromBoolean(mode == BrowsingMode.Private)
        tabsUseCases?.addTab?.invoke(url, private = (mode == BrowsingMode.Private))
    }

    private fun navigateToGivenDirection(directions: NavDirections) {
        navController.nav(R.id.bookmarkFragment, directions)
    }
}
