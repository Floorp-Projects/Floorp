/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_bookmark.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.share
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.utils.ItsNotBrokenSnack
import kotlin.coroutines.CoroutineContext

class BookmarkFragment : Fragment(), CoroutineScope, BackHandler {

    private lateinit var job: Job
    private lateinit var bookmarkComponent: BookmarkComponent
    private lateinit var currentRoot: BookmarkNode

    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_bookmark, container, false)
        bookmarkComponent = BookmarkComponent(view.bookmark_layout, ActionBusFactory.get(this))
        return view
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.library_menu, menu)
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
                                val activity = requireActivity() as HomeActivity
                                Navigation.findNavController(activity, R.id.container)
                                    .navigate(BookmarkFragmentDirections.actionBookmarkFragmentToBrowserFragment(null))
                                if (activity.browsingModeManager.isPrivate) {
                                    requireComponents.useCases.tabsUseCases.addPrivateTab.invoke(url)
                                    activity.browsingModeManager.mode =
                                        BrowsingModeManager.Mode.Private
                                } else {
                                    requireComponents.useCases.sessionUseCases.loadUrl.invoke(url)
                                    activity.browsingModeManager.mode =
                                        BrowsingModeManager.Mode.Normal
                                }
                            }
                        }
                    }
                    is BookmarkAction.Expand -> {
                        Navigation.findNavController(requireActivity(), R.id.container)
                            .navigate(BookmarkFragmentDirections.actionBookmarkFragmentSelf(it.folder.guid))
                    }
                    is BookmarkAction.BackPressed -> {
                        Navigation.findNavController(requireActivity(), R.id.container).popBackStack()
                    }
                    is BookmarkAction.Edit -> {
                        ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1238")
                    }
                    is BookmarkAction.Select -> {
                        ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1239")
                    }
                    is BookmarkAction.Copy -> {
                        ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1239")
                    }
                    is BookmarkAction.Share -> {
                        it.item.url?.let { url -> requireContext().share(url) }
                    }
                    is BookmarkAction.OpenInNewTab -> {
                        it.item.url?.let { url ->
                            requireComponents.useCases.tabsUseCases.addTab.invoke(url)
                            (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Normal
                        }
                    }
                    is BookmarkAction.OpenInPrivateTab -> {
                        it.item.url?.let { url ->
                            requireComponents.useCases.tabsUseCases.addPrivateTab.invoke(url)
                            (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Private
                        }
                    }
                    is BookmarkAction.Delete -> {
                        launch(IO) {
                            requireComponents.core.bookmarksStorage.deleteNode(it.item.guid)
                            requireComponents.core.bookmarksStorage.getTree(currentRoot.guid, false)
                                ?.let { node ->
                                    getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.Change(node))
                                }
                        }
                    }
                    is BookmarkAction.ExitSelectMode -> {
                        ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1239")
                    }
                }
            }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.libraryClose -> {
                Navigation.findNavController(requireActivity(), R.id.container)
                    .popBackStack(R.id.libraryFragment, true)
                true
            }
            R.id.librarySearch -> {
                // TODO Library Search
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val currentGuid = BookmarkFragmentArgs.fromBundle(arguments!!).currentRoot.ifEmpty { BookmarkRoot.Root.id }

        launch(IO) {
            currentRoot = requireComponents.core.bookmarksStorage.getTree(currentGuid) as BookmarkNode

            launch(Main) {
                getManagedEmitter<BookmarkChange>().onNext(BookmarkChange.Change(currentRoot))
            }
        }
    }

    override fun onBackPressed(): Boolean = (bookmarkComponent.uiView as BookmarkUIView).onBackPressed()
}
