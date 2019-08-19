/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.graphics.PorterDuff.Mode.SRC_IN
import android.graphics.PorterDuffColorFilter
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_bookmark.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.sync.AccountObserver
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
import org.mozilla.fenix.ext.setRootTitles
import org.mozilla.fenix.ext.urlToTrimmedHost
import org.mozilla.fenix.ext.withOptionalDesktopFolders
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.utils.allowUndo

@SuppressWarnings("TooManyFunctions", "LargeClass")
class BookmarkFragment : LibraryPageFragment<BookmarkNode>(), BackHandler, AccountObserver {

    private lateinit var bookmarkStore: BookmarkStore
    private lateinit var bookmarkView: BookmarkView
    private lateinit var signInView: SignInView
    private lateinit var bookmarkInteractor: BookmarkFragmentInteractor

    private val sharedViewModel: BookmarksSharedViewModel by activityViewModels()

    var currentRoot: BookmarkNode? = null
    private val navigation by lazy { findNavController() }
    private val onDestinationChangedListener =
        NavController.OnDestinationChangedListener { _, destination, args ->
            if (destination.id != R.id.bookmarkFragment ||
                args != null && BookmarkFragmentArgs.fromBundle(args).currentRoot != currentRoot?.guid) {

                bookmarkInteractor.onAllBookmarksDeselected()
            }
        }
    lateinit var initialJob: Job
    private var pendingBookmarkDeletionJob: (suspend () -> Unit)? = null
    private var pendingBookmarksToDelete: MutableSet<BookmarkNode> = mutableSetOf()

    private val metrics
        get() = context?.components?.analytics?.metrics

    override val selectedItems get() = bookmarkStore.state.mode.selectedItems

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_bookmark, container, false)

        bookmarkStore = StoreProvider.get(this) {
            BookmarkStore(BookmarkState(null))
        }
        bookmarkInteractor = BookmarkFragmentInteractor(
            bookmarkStore = bookmarkStore,
            viewModel = sharedViewModel,
            bookmarksController = DefaultBookmarkController(
                context = context!!,
                navController = findNavController(),
                snackbarPresenter = FenixSnackbarPresenter(view),
                deleteBookmarkNodes = ::deleteMulti
            ),
            metrics = metrics!!
        )

        bookmarkView = BookmarkView(view.bookmarkLayout, bookmarkInteractor)
        signInView = SignInView(view.bookmarkLayout, bookmarkInteractor)
        return view
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(bookmarkStore) {
            bookmarkView.update(it)
        }
        sharedViewModel.apply {
            signedIn.observe(this@BookmarkFragment, Observer<Boolean> {
                signInView.update(it)
            })
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        activity?.title = getString(R.string.library_bookmarks)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        context?.let { setRootTitles(it) }

        (activity as? AppCompatActivity)?.supportActionBar?.show()
        checkIfSignedIn()

        navigation.addOnDestinationChangedListener(onDestinationChangedListener)
        val currentGuid = BookmarkFragmentArgs.fromBundle(arguments!!).currentRoot.ifEmpty { BookmarkRoot.Mobile.id }

        initialJob = loadInitialBookmarkFolder(currentGuid)
    }

    private fun loadInitialBookmarkFolder(currentGuid: String): Job {
        return viewLifecycleOwner.lifecycleScope.launch(IO) {
            currentRoot =
                context?.bookmarkStorage()?.getTree(currentGuid).withOptionalDesktopFolders(context) as BookmarkNode

            if (!isActive) return@launch
            launch(Main) {
                bookmarkInteractor.onBookmarksChanged(currentRoot!!)
                sharedViewModel.selectedFolder = currentRoot
            }
        }
    }

    private fun checkIfSignedIn() {
        context?.components?.backgroundServices?.accountManager?.let {
            it.register(this, owner = this)
            it.authenticatedAccount()?.let { bookmarkInteractor.onSignedIn() }
                ?: bookmarkInteractor.onSignedOut()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        navigation.removeOnDestinationChangedListener(onDestinationChangedListener)
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        when (val mode = bookmarkStore.state.mode) {
            BookmarkState.Mode.Normal -> {
                inflater.inflate(R.menu.bookmarks_menu, menu)
            }
            is BookmarkState.Mode.Selecting -> {
                if (mode.selectedItems.any { it.type != BookmarkNodeType.ITEM }) {
                    inflater.inflate(R.menu.bookmarks_select_multi_not_item, menu)
                } else {
                    inflater.inflate(R.menu.bookmarks_select_multi, menu)
                }
                menu.findItem(R.id.edit_bookmark_multi_select)?.run {
                    isVisible = mode.selectedItems.size == 1
                    icon.colorFilter = PorterDuffColorFilter(
                        ContextCompat.getColor(context!!, R.color.white_color),
                        SRC_IN
                    )
                }
            }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.libraryClose -> {
                close()
                true
            }
            R.id.add_bookmark_folder -> {
                nav(
                    R.id.bookmarkFragment,
                    BookmarkFragmentDirections
                        .actionBookmarkFragmentToBookmarkAddFolderFragment()
                )
                true
            }
            R.id.open_bookmarks_in_new_tabs_multi_select -> {
                openItemsInNewTab { node -> node.url }

                nav(R.id.bookmarkFragment, BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                metrics?.track(Event.OpenedBookmarksInNewTabs)
                true
            }
            R.id.open_bookmarks_in_private_tabs_multi_select -> {
                openItemsInNewTab(private = true) { node -> node.url }

                nav(R.id.bookmarkFragment, BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                metrics?.track(Event.OpenedBookmarksInPrivateTabs)
                true
            }
            R.id.edit_bookmark_multi_select -> {
                val bookmark = bookmarkStore.state.mode.selectedItems.first()
                nav(
                    R.id.bookmarkFragment,
                    BookmarkFragmentDirections
                        .actionBookmarkFragmentToBookmarkEditFragment(bookmark.guid)
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

    override fun onBackPressed(): Boolean = bookmarkView.onBackPressed()

    override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
        bookmarkInteractor.onSignedIn()
        lifecycleScope.launch {
            refreshBookmarks()
        }
    }

    override fun onLoggedOut() {
        bookmarkInteractor.onSignedOut()
    }

    private suspend fun refreshBookmarks() {
        // The bookmark tree in our 'state' can be null - meaning, no bookmark tree has been selected.
        // If that's the case, we don't know what node to refresh, and so we bail out.
        // See https://github.com/mozilla-mobile/fenix/issues/4671
        val currentGuid = bookmarkStore.state.tree?.guid ?: return
        context?.bookmarkStorage()?.getTree(currentGuid, false).withOptionalDesktopFolders(context)
            ?.let { node ->
                var rootNode = node
                pendingBookmarksToDelete.forEach {
                    rootNode -= it.guid
                }
                bookmarkInteractor.onBookmarksChanged(rootNode)
            }
    }

    override fun onPause() {
        invokePendingDeletion()
        super.onPause()
    }

    private suspend fun deleteSelectedBookmarks(selected: Set<BookmarkNode>) {
        selected.forEach {
            context?.bookmarkStorage()?.deleteNode(it.guid)
        }
    }

    private fun deleteMulti(selected: Set<BookmarkNode>, eventType: Event = Event.RemoveBookmarks) {
        pendingBookmarksToDelete.addAll(selected)

        var bookmarkTree = currentRoot
        pendingBookmarksToDelete.forEach {
            bookmarkTree -= it.guid
        }
        bookmarkInteractor.onBookmarksChanged(bookmarkTree!!)

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
                    bookmarkNode.url?.urlToTrimmedHost(context!!) ?: bookmarkNode.title
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
