/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDirections
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_bookmark.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.minus
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.utils.allowUndo

@Suppress("TooManyFunctions", "LargeClass")
class BookmarkFragment : LibraryPageFragment<BookmarkNode>(), BackHandler {

    private lateinit var bookmarkStore: BookmarkFragmentStore
    private lateinit var bookmarkView: BookmarkView
    private lateinit var bookmarkInteractor: BookmarkFragmentInteractor

    private val sharedViewModel: BookmarksSharedViewModel by activityViewModels {
        ViewModelProvider.NewInstanceFactory() // this is a workaround for #4652
    }
    private val desktopFolders by lazy { DesktopFolders(context!!, showMobileRoot = false) }

    lateinit var initialJob: Job
    private var pendingBookmarkDeletionJob: (suspend () -> Unit)? = null
    private var pendingBookmarksToDelete: MutableSet<BookmarkNode> = mutableSetOf()
    private val refreshOnSignInListener = object : AccountObserver {
        override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
            lifecycleScope.launch { refreshBookmarks() }
        }
    }

    private val metrics
        get() = context?.components?.analytics?.metrics

    override val selectedItems get() = bookmarkStore.state.mode.selectedItems

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_bookmark, container, false)

        bookmarkStore = StoreProvider.get(this) {
            BookmarkFragmentStore(BookmarkFragmentState(null))
        }
        bookmarkInteractor = BookmarkFragmentInteractor(
            bookmarkStore = bookmarkStore,
            viewModel = sharedViewModel,
            bookmarksController = DefaultBookmarkController(
                context = context!!,
                navController = findNavController(),
                snackbarPresenter = FenixSnackbarPresenter(view),
                deleteBookmarkNodes = ::deleteMulti,
                invokePendingDeletion = ::invokePendingDeletion
            ),
            metrics = metrics!!
        )

        bookmarkView = BookmarkView(view.bookmarkLayout, bookmarkInteractor)

        val signInView = SignInView(view.bookmarkLayout, findNavController())
        sharedViewModel.signedIn.observe(viewLifecycleOwner, signInView)

        lifecycle.addObserver(
            BookmarkDeselectNavigationListener(
                findNavController(),
                sharedViewModel,
                bookmarkInteractor
            )
        )

        return view
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(bookmarkStore) {
            bookmarkView.update(it)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()

        (activity as? AppCompatActivity)?.supportActionBar?.show()
        context?.components?.backgroundServices?.accountManager?.let { accountManager ->
            sharedViewModel.observeAccountManager(accountManager, owner = this)
            accountManager.register(refreshOnSignInListener, owner = this)
        }

        val currentGuid = BookmarkFragmentArgs.fromBundle(arguments!!).currentRoot.ifEmpty { BookmarkRoot.Mobile.id }

        initialJob = loadInitialBookmarkFolder(currentGuid)
    }

    private fun loadInitialBookmarkFolder(currentGuid: String): Job {
        return viewLifecycleOwner.lifecycleScope.launch(Main) {
            val currentRoot = withContext(IO) {
                requireContext().bookmarkStorage
                    .getTree(currentGuid)
                    ?.let { desktopFolders.withOptionalDesktopFolders(it) }!!
            }

            if (isActive) {
                bookmarkInteractor.onBookmarksChanged(currentRoot)
                sharedViewModel.selectedFolder = currentRoot
            }
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        when (val mode = bookmarkStore.state.mode) {
            BookmarkFragmentState.Mode.Normal -> {
                inflater.inflate(R.menu.bookmarks_menu, menu)
            }
            is BookmarkFragmentState.Mode.Selecting -> {
                if (mode.selectedItems.any { it.type != BookmarkNodeType.ITEM }) {
                    inflater.inflate(R.menu.bookmarks_select_multi_not_item, menu)
                } else {
                    inflater.inflate(R.menu.bookmarks_select_multi, menu)
                }

                menu.findItem(R.id.share_bookmark_multi_select)?.isVisible = mode.selectedItems.size == 1
            }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.add_bookmark_folder -> {
                navigate(
                    BookmarkFragmentDirections
                        .actionBookmarkFragmentToBookmarkAddFolderFragment()
                )
                true
            }
            R.id.open_bookmarks_in_new_tabs_multi_select -> {
                openItemsInNewTab { node -> node.url }

                navigate(BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                metrics?.track(Event.OpenedBookmarksInNewTabs)
                true
            }
            R.id.open_bookmarks_in_private_tabs_multi_select -> {
                openItemsInNewTab(private = true) { node -> node.url }

                navigate(BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                metrics?.track(Event.OpenedBookmarksInPrivateTabs)
                true
            }
            R.id.share_bookmark_multi_select -> {
                val bookmark = bookmarkStore.state.mode.selectedItems.first()
                navigate(
                    BookmarkFragmentDirections.actionBookmarkFragmentToShareFragment(
                        data = arrayOf(ShareData(url = bookmark.url, title = bookmark.title))
                    )
                )
                true
            }
            R.id.delete_bookmarks_multi_select -> {
                deleteMulti(bookmarkStore.state.mode.selectedItems)
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun navigate(directions: NavDirections) {
        invokePendingDeletion()
        nav(R.id.bookmarkFragment, directions)
    }

    override fun onBackPressed(): Boolean {
        invokePendingDeletion()
        return bookmarkView.onBackPressed()
    }

    private suspend fun refreshBookmarks() {
        // The bookmark tree in our 'state' can be null - meaning, no bookmark tree has been selected.
        // If that's the case, we don't know what node to refresh, and so we bail out.
        // See https://github.com/mozilla-mobile/fenix/issues/4671
        val currentGuid = bookmarkStore.state.tree?.guid ?: return
        context?.bookmarkStorage
            ?.getTree(currentGuid, false)
            ?.let { desktopFolders.withOptionalDesktopFolders(it) }
            ?.let { node ->
                val rootNode = node - pendingBookmarksToDelete
                bookmarkInteractor.onBookmarksChanged(rootNode)
            }
    }

    override fun onPause() {
        invokePendingDeletion()
        super.onPause()
    }

    private suspend fun deleteSelectedBookmarks(selected: Set<BookmarkNode>) {
        CoroutineScope(IO).launch {
            val tempStorage = context?.bookmarkStorage
            selected.map {
                async { tempStorage?.deleteNode(it.guid) }
            }.awaitAll()
        }
    }

    private fun deleteMulti(selected: Set<BookmarkNode>, eventType: Event = Event.RemoveBookmarks) {
        pendingBookmarksToDelete.addAll(selected)

        val bookmarkTree = sharedViewModel.selectedFolder!! - pendingBookmarksToDelete
        bookmarkInteractor.onBookmarksChanged(bookmarkTree)

        val deleteOperation: (suspend () -> Unit) = {
            deleteSelectedBookmarks(selected)
            pendingBookmarkDeletionJob = null
            // Since this runs in a coroutine, we can't depend upon the fragment still being attached
            metrics?.track(Event.RemoveBookmarks)
            refreshBookmarks()
        }

        pendingBookmarkDeletionJob = deleteOperation

        val message = when (eventType) {
            is Event.RemoveBookmarks -> {
                getString(R.string.bookmark_deletion_multiple_snackbar_message)
            }
            is Event.RemoveBookmarkFolder,
            is Event.RemoveBookmark -> {
                val bookmarkNode = selected.first()
                getString(
                    R.string.bookmark_deletion_snackbar_message,
                    bookmarkNode.url?.toShortUrl(context!!.components.publicSuffixList) ?: bookmarkNode.title
                )
            }
            else -> throw IllegalStateException("Illegal event type in onDeleteSome")
        }

        lifecycleScope.allowUndo(
            view!!, message,
            getString(R.string.bookmark_undo_deletion), {
                pendingBookmarksToDelete.removeAll(selected)
                pendingBookmarkDeletionJob = null
                refreshBookmarks()
            }, operation = deleteOperation
        )
    }

    private fun invokePendingDeletion() {
        pendingBookmarkDeletionJob?.let {
            viewLifecycleOwner.lifecycleScope.launch {
                it.invoke()
            }.invokeOnCompletion {
                pendingBookmarkDeletionJob = null
            }
        }
    }
}
