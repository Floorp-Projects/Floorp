/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.PorterDuff
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
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProviders
import androidx.navigation.NavController
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_bookmark.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.allowUndo
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.urlToTrimmedHost
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import kotlin.coroutines.CoroutineContext

@SuppressWarnings("TooManyFunctions")
class BookmarkFragment : Fragment(), CoroutineScope, BackHandler, AccountObserver {

    private lateinit var job: Job
    private lateinit var bookmarkComponent: BookmarkComponent
    private lateinit var signInComponent: SignInComponent
    var currentRoot: BookmarkNode? = null
    private val navigation by lazy { Navigation.findNavController(requireView()) }
    private val onDestinationChangedListener =
        NavController.OnDestinationChangedListener { _, destination, args ->
            if (destination.id != R.id.bookmarkFragment ||
                args != null && BookmarkFragmentArgs.fromBundle(args).currentRoot != currentRoot?.guid
            )
                getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.ClearSelection)
        }
    lateinit var initialJob: Job

    override val coroutineContext: CoroutineContext
        get() = Main + job

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_bookmark, container, false)
        bookmarkComponent = BookmarkComponent(
            view.bookmark_layout,
            ActionBusFactory.get(this),
            FenixViewModelProvider.create(
                this,
                BookmarkViewModel::class.java,
                BookmarkViewModel.Companion::create
            )
        )
        signInComponent = SignInComponent(
            view.bookmark_layout,
            ActionBusFactory.get(this),
            FenixViewModelProvider.create(
                this,
                SignInViewModel::class.java
            ) {
                SignInViewModel(SignInState(false))
            }
        )
        return view
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()
        activity?.title = getString(R.string.library_bookmarks)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        (activity as? AppCompatActivity)?.supportActionBar?.show()
        checkIfSignedIn()

        navigation.addOnDestinationChangedListener(onDestinationChangedListener)
        val currentGuid = BookmarkFragmentArgs.fromBundle(arguments!!).currentRoot.ifEmpty { BookmarkRoot.Mobile.id }

        initialJob = loadInitialBookmarkFolder(currentGuid)
    }

    private fun loadInitialBookmarkFolder(currentGuid: String): Job {
        return launch(IO) {
            currentRoot = requireComponents.core.bookmarksStorage.getTree(currentGuid) as BookmarkNode

            launch(Main) {
                getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.Change(currentRoot!!))

                activity?.run {
                    ViewModelProviders.of(this).get(BookmarksSharedViewModel::class.java)
                }!!.selectedFolder = null
            }
        }
    }

    private fun checkIfSignedIn() {
        val accountManager = requireComponents.backgroundServices.accountManager
        accountManager.register(this, owner = this)
        accountManager.authenticatedAccount()?.let { getManagedEmitter<SignInChange>().onNext(SignInChange.SignedIn) }
            ?: getManagedEmitter<SignInChange>().onNext(SignInChange.SignedOut)
    }

    override fun onDestroy() {
        super.onDestroy()
        navigation.removeOnDestinationChangedListener(onDestinationChangedListener)
        job.cancel()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        when (val mode = (bookmarkComponent.uiView as BookmarkUIView).mode) {
            BookmarkState.Mode.Normal -> {
                inflater.inflate(R.menu.library_menu, menu)
            }
            is BookmarkState.Mode.Selecting -> {
                inflater.inflate(R.menu.bookmarks_select_multi, menu)
                menu.findItem(R.id.edit_bookmark_multi_select)?.run {
                    isVisible = mode.selectedItems.size == 1
                    icon.colorFilter = PorterDuffColorFilter(
                        ContextCompat.getColor(context!!, R.color.white_color),
                        PorterDuff.Mode.SRC_IN
                    )
                }
            }
        }
    }

    @SuppressWarnings("ComplexMethod")
    override fun onStart() {
        super.onStart()
        getAutoDisposeObservable<BookmarkAction>()
            .subscribe {
                when (it) {
                    is BookmarkAction.Open -> {
                        if (it.item.type == BookmarkNodeType.ITEM) {
                            it.item.url?.let { url ->
                                (activity as HomeActivity)
                                    .openToBrowserAndLoad(
                                        searchTermOrURL = url,
                                        newTab = false,
                                        from = BrowserDirection.FromBookmarks
                                    )
                            }
                        }
                        requireComponents.analytics.metrics.track(Event.OpenedBookmark)
                    }
                    is BookmarkAction.Expand -> {
                        navigation
                            .navigate(BookmarkFragmentDirections.actionBookmarkFragmentSelf(it.folder.guid))
                    }
                    is BookmarkAction.BackPressed -> {
                        navigation.popBackStack()
                    }
                    is BookmarkAction.Edit -> {
                        navigation
                            .navigate(
                                BookmarkFragmentDirections
                                    .actionBookmarkFragmentToBookmarkEditFragment(it.item.guid)
                            )
                    }
                    is BookmarkAction.Select -> {
                        getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.IsSelected(it.item))
                    }
                    is BookmarkAction.Deselect -> {
                        getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.IsDeselected(it.item))
                    }
                    is BookmarkAction.Copy -> {
                        it.item.copyUrl(context!!)
                        FenixSnackbar.make(view!!, FenixSnackbar.LENGTH_LONG)
                            .setText(context!!.getString(R.string.url_copied)).show()
                        requireComponents.analytics.metrics.track(Event.CopyBookmark)
                    }
                    is BookmarkAction.Share -> {
                        it.item.url?.apply {
                            navigation
                                .navigate(
                                    BookmarkFragmentDirections.actionBookmarkFragmentToShareFragment(this)
                                )
                            requireComponents.analytics.metrics.track(Event.ShareBookmark)
                        }
                    }
                    is BookmarkAction.OpenInNewTab -> {
                        it.item.url?.let { url ->
                            (activity as HomeActivity).browsingModeManager.mode =
                                BrowsingModeManager.Mode.Normal
                            (activity as HomeActivity).openToBrowserAndLoad(
                                searchTermOrURL = url,
                                newTab = true,
                                from = BrowserDirection.FromBookmarks
                            )
                            requireComponents.analytics.metrics.track(Event.OpenedBookmarkInNewTab)
                        }
                    }
                    is BookmarkAction.OpenInPrivateTab -> {
                        it.item.url?.let { url ->
                            (activity as HomeActivity).browsingModeManager.mode =
                                BrowsingModeManager.Mode.Private
                            (activity as HomeActivity).openToBrowserAndLoad(
                                searchTermOrURL = url,
                                newTab = true,
                                from = BrowserDirection.FromBookmarks
                            )
                            requireComponents.analytics.metrics.track(Event.OpenedBookmarkInPrivateTab)
                        }
                    }
                    is BookmarkAction.Delete -> {
                        val components = context?.applicationContext?.components!!

                        getManagedEmitter<BookmarkChange>()
                            .onNext(BookmarkChange.Change(currentRoot - it.item.guid))

                        CoroutineScope(Main).allowUndo(
                            view!!,
                            getString(R.string.bookmark_deletion_snackbar_message, it.item.url.urlToTrimmedHost()),
                            getString(R.string.bookmark_undo_deletion), { refreshBookmarks(components) }
                        ) {
                            components.core.bookmarksStorage.deleteNode(it.item.guid)
                            components.analytics.metrics.track(Event.RemoveBookmark)
                            refreshBookmarks(components)
                        }
                    }
                    is BookmarkAction.SwitchMode -> activity?.invalidateOptionsMenu()
                }
            }

        getAutoDisposeObservable<SignInAction>()
            .subscribe {
                when (it) {
                    is SignInAction.ClickedSignIn -> {
                        requireComponents.services.accountsAuthFeature.beginAuthentication()
                        (activity as HomeActivity).openToBrowser(BrowserDirection.FromBookmarks)
                    }
                }
            }
    }

    @SuppressWarnings("ComplexMethod")
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.libraryClose -> {
                navigation
                    .popBackStack(R.id.libraryFragment, true)
                true
            }
            R.id.open_bookmarks_in_new_tabs_multi_select -> {
                getSelectedBookmarks().forEach { node ->
                    node.url?.let {
                        requireComponents.useCases.tabsUseCases.addTab.invoke(it)
                    }
                }

                (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Normal
                (activity as HomeActivity).supportActionBar?.hide()
                navigation
                    .navigate(BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                requireComponents.analytics.metrics.track(Event.OpenedBookmarksInNewTabs)
                true
            }
            R.id.edit_bookmark_multi_select -> {
                val bookmark = getSelectedBookmarks().first()
                navigation
                    .navigate(
                        BookmarkFragmentDirections
                            .actionBookmarkFragmentToBookmarkEditFragment(bookmark.guid)
                    )
                true
            }
            R.id.open_bookmarks_in_private_tabs_multi_select -> {
                getSelectedBookmarks().forEach { node ->
                    node.url?.let {
                        requireComponents.useCases.tabsUseCases.addPrivateTab.invoke(it)
                    }
                }

                (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Private
                (activity as HomeActivity).supportActionBar?.hide()
                navigation
                    .navigate(BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                requireComponents.analytics.metrics.track(Event.OpenedBookmarksInPrivateTabs)
                true
            }
            R.id.delete_bookmarks_multi_select -> {
                val components = context?.applicationContext?.components!!

                val selectedBookmarks = getSelectedBookmarks()
                getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.Change(currentRoot - selectedBookmarks))

                CoroutineScope(Main).allowUndo(
                    view!!, getString(R.string.bookmark_deletion_multiple_snackbar_message),
                    getString(R.string.bookmark_undo_deletion), { refreshBookmarks(components) }
                ) {
                    deleteSelectedBookmarks(selectedBookmarks, components)
                    components.analytics.metrics.track(Event.RemoveBookmarks)
                    refreshBookmarks(components)
                }
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onBackPressed(): Boolean = (bookmarkComponent.uiView as BookmarkUIView).onBackPressed()

    override fun onAuthenticated(account: OAuthAccount) {
        getManagedEmitter<SignInChange>().onNext(SignInChange.SignedIn)
    }

    override fun onError(error: Exception) {
    }

    override fun onLoggedOut() {
        getManagedEmitter<SignInChange>().onNext(SignInChange.SignedOut)
    }

    override fun onProfileUpdated(profile: Profile) {
    }

    fun getBookmarks() = (bookmarkComponent.uiView as BookmarkUIView).tree?.children
    fun getSelectedBookmarks() = (bookmarkComponent.uiView as BookmarkUIView).getSelected()

    private suspend fun deleteSelectedBookmarks(
        selected: Set<BookmarkNode> = getSelectedBookmarks(),
        components: Components = requireComponents
    ) {
        selected.forEach {
            components.core.bookmarksStorage.deleteNode(it.guid)
        }
    }

    private suspend fun refreshBookmarks(components: Components = requireComponents) {
        components.core.bookmarksStorage.getTree(currentRoot!!.guid, false)
            ?.let { node ->
                getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.Change(node))
            }
    }

    private fun BookmarkNode.copyUrl(context: Context) {
        val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clipBoard.primaryClip = ClipData.newPlainText(url, url)
    }
}

operator fun BookmarkNode?.minus(child: String): BookmarkNode {
    return this!!.copy(children = this.children?.filter { it.guid != child })
}

operator fun BookmarkNode?.minus(children: Set<BookmarkNode>): BookmarkNode {
    return this!!.copy(children = this.children?.filter { it !in children })
}
