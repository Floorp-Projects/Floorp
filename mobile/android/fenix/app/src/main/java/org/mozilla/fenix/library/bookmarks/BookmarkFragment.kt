/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.view.menu.ActionMenuItemView
import androidx.appcompat.widget.ActionMenuView
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProviders
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
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.allowUndo
import org.mozilla.fenix.ext.getColorIntFromAttr
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.share
import org.mozilla.fenix.ext.urlToHost
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import kotlin.coroutines.CoroutineContext

@SuppressWarnings("TooManyFunctions")
class BookmarkFragment : Fragment(), CoroutineScope, BackHandler, AccountObserver {

    private lateinit var job: Job
    private lateinit var bookmarkComponent: BookmarkComponent
    private lateinit var signInComponent: SignInComponent
    private var currentRoot: BookmarkNode? = null

    override val coroutineContext: CoroutineContext
        get() = Main + job

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_bookmark, container, false)
        bookmarkComponent = BookmarkComponent(view.bookmark_layout, ActionBusFactory.get(this))
        signInComponent = SignInComponent(view.bookmark_layout, ActionBusFactory.get(this))
        return view
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()
        (activity as AppCompatActivity).title = getString(R.string.library_bookmarks)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.show()
        checkIfSignedIn()

        val currentGuid = BookmarkFragmentArgs.fromBundle(arguments!!).currentRoot.ifEmpty { BookmarkRoot.Root.id }

        launch(IO) {
            currentRoot = requireComponents.core.bookmarksStorage.getTree(currentGuid) as BookmarkNode

            launch(Main) {
                if (currentGuid != BookmarkRoot.Root.id) (activity as HomeActivity).title = currentRoot!!.title
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

    fun setToolbarColors(foreground: Int, background: Int) {
        val toolbar = (activity as AppCompatActivity).findViewById<Toolbar>(R.id.navigationToolbar)
        val colorFilter = PorterDuffColorFilter(
            ContextCompat.getColor(context!!, foreground), PorterDuff.Mode.SRC_IN
        )
        toolbar.setBackgroundColor(ContextCompat.getColor(context!!, background))
        toolbar.setTitleTextColor(ContextCompat.getColor(context!!, foreground))

        themeToolbar(
            toolbar, foreground,
            background, colorFilter
        )
    }

    override fun onStop() {
        super.onStop()
        // Reset the toolbar color
        setToolbarColors(
            R.attr.primaryText.getColorIntFromAttr(context!!),
            R.attr.foundation.getColorIntFromAttr(context!!)
        )
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        val toolbar = (activity as AppCompatActivity).findViewById<Toolbar>(R.id.navigationToolbar)
        when (val mode = (bookmarkComponent.uiView as BookmarkUIView).mode) {
            BookmarkState.Mode.Normal -> {
                inflater.inflate(R.menu.library_menu, menu)
                activity?.title =
                    if (currentRoot?.title in setOf(
                            "root",
                            null
                        )
                    ) getString(R.string.library_bookmarks) else currentRoot!!.title

                setToolbarColors(
                    R.attr.primaryText.getColorIntFromAttr(context!!),
                    R.attr.foundation.getColorIntFromAttr(context!!)
                )
            }
            is BookmarkState.Mode.Selecting -> {
                inflater.inflate(R.menu.bookmarks_select_multi, menu)

                menu.findItem(R.id.edit_bookmark_multi_select).run {
                    isVisible = mode.selectedItems.size == 1
                    icon.colorFilter = PorterDuffColorFilter(
                        ContextCompat.getColor(context!!, R.color.white_color),
                        PorterDuff.Mode.SRC_IN
                    )
                }

                activity?.title = getString(R.string.bookmarks_multi_select_title, mode.selectedItems.size)
                setToolbarColors(
                    R.color.white_color,
                    R.attr.accentBright.getColorIntFromAttr(context!!)
                )
            }
        }
    }

    private fun themeToolbar(
        toolbar: Toolbar,
        textColor: Int,
        backgroundColor: Int,
        colorFilter: PorterDuffColorFilter? = null
    ) {
        toolbar.setTitleTextColor(ContextCompat.getColor(context!!, textColor))
        toolbar.setBackgroundColor(ContextCompat.getColor(context!!, backgroundColor))

        if (colorFilter == null) {
            return
        }

        toolbar.overflowIcon?.colorFilter = colorFilter
        (0 until toolbar.childCount).forEach {
            when (val item = toolbar.getChildAt(it)) {
                is ImageButton -> item.drawable.colorFilter = colorFilter
                is ActionMenuView -> themeActionMenuView(item, colorFilter)
            }
        }
    }

    private fun themeActionMenuView(
        item: ActionMenuView,
        colorFilter: PorterDuffColorFilter
    ) {
        (0 until item.childCount).forEach {
            val innerChild = item.getChildAt(it)
            if (innerChild is ActionMenuItemView) {
                themeChildren(innerChild, item, colorFilter)
            }
        }
    }

    private fun themeChildren(
        innerChild: ActionMenuItemView,
        item: ActionMenuView,
        colorFilter: PorterDuffColorFilter
    ) {
        val drawables = innerChild.compoundDrawables
        for (k in drawables.indices) {
            drawables[k]?.let {
                item.post { innerChild.compoundDrawables[k].colorFilter = colorFilter }
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
                                    .openToBrowserAndLoad(url, from = BrowserDirection.FromBookmarks)
                            }
                        }
                        requireComponents.analytics.metrics.track(Event.OpenedBookmark)
                    }
                    is BookmarkAction.Expand -> {
                        Navigation.findNavController(requireActivity(), R.id.container)
                            .navigate(BookmarkFragmentDirections.actionBookmarkFragmentSelf(it.folder.guid))
                    }
                    is BookmarkAction.BackPressed -> {
                        Navigation.findNavController(requireActivity(), R.id.container).popBackStack()
                    }
                    is BookmarkAction.Edit -> {
                        Navigation.findNavController(requireActivity(), R.id.container)
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
                            requireContext().share(this)
                            requireComponents.analytics.metrics.track(Event.ShareBookmark)
                        }
                    }
                    is BookmarkAction.OpenInNewTab -> {
                        it.item.url?.let { url ->
                            requireComponents.useCases.tabsUseCases.addTab.invoke(url)
                            (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Normal
                            requireComponents.analytics.metrics.track(Event.OpenedBookmarkInNewTab)
                        }
                    }
                    is BookmarkAction.OpenInPrivateTab -> {
                        it.item.url?.let { url ->
                            requireComponents.useCases.tabsUseCases.addPrivateTab.invoke(url)
                            (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Private
                            requireComponents.analytics.metrics.track(Event.OpenedBookmarkInPrivateTab)
                        }
                    }
                    is BookmarkAction.Delete -> {
                        allowUndo(
                            view!!, getString(R.string.bookmark_deletion_snackbar_message, it.item.url.urlToHost()),
                            getString(R.string.bookmark_undo_deletion)
                        ) {
                            requireComponents.core.bookmarksStorage.deleteNode(it.item.guid)
                            requireComponents.analytics.metrics.track(Event.RemoveBookmark)
                            refreshBookmarks()
                        }
                    }
                    is BookmarkAction.ModeChanged -> activity?.invalidateOptionsMenu()
                }
            }

        getAutoDisposeObservable<SignInAction>()
            .subscribe {
                when (it) {
                    is SignInAction.ClickedSignIn -> {
                        requireComponents.services.accountsAuthFeature.beginAuthentication()
                        (activity as HomeActivity).openToBrowser(null, from = BrowserDirection.FromBookmarks)
                    }
                }
            }
    }

    @SuppressWarnings("ComplexMethod")
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.libraryClose -> {
                Navigation.findNavController(requireActivity(), R.id.container)
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
                Navigation.findNavController(requireActivity(), R.id.container)
                    .navigate(BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                requireComponents.analytics.metrics.track(Event.OpenedBookmarksInNewTabs)
                true
            }
            R.id.edit_bookmark_multi_select -> {
                val bookmark = getSelectedBookmarks().first()
                Navigation.findNavController(requireActivity(), R.id.container)
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
                Navigation.findNavController(requireActivity(), R.id.container)
                    .navigate(BookmarkFragmentDirections.actionBookmarkFragmentToHomeFragment())
                requireComponents.analytics.metrics.track(Event.OpenedBookmarksInPrivateTabs)
                true
            }
            R.id.delete_bookmarks_multi_select -> {
                allowUndo(
                    view!!, getString(R.string.bookmark_deletion_multiple_snackbar_message),
                    getString(R.string.bookmark_undo_deletion)
                ) {
                    deleteSelectedBookmarks()
                    requireComponents.analytics.metrics.track(Event.RemoveBookmarks)
                    refreshBookmarks()
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

    private fun getSelectedBookmarks() = (bookmarkComponent.uiView as BookmarkUIView).getSelected()

    private suspend fun deleteSelectedBookmarks() {
        getSelectedBookmarks().forEach {
            requireComponents.core.bookmarksStorage.deleteNode(it.guid)
        }
    }

    private suspend fun refreshBookmarks() {
        requireComponents.core.bookmarksStorage.getTree(currentRoot!!.guid, false)
            ?.let { node ->
                getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.Change(node))
            }
    }

    private fun BookmarkNode.copyUrl(context: Context) {
        val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val uri = Uri.parse(url)
        clipBoard.primaryClip = ClipData.newRawUri("Uri", uri)
    }
}
