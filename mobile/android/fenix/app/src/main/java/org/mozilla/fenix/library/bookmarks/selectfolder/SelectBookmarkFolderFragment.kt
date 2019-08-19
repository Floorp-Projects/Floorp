/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.selectfolder

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
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.fragment_select_bookmark_folder.*
import kotlinx.android.synthetic.main.fragment_select_bookmark_folder.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setRootTitles
import org.mozilla.fenix.ext.withOptionalDesktopFolders
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel
import org.mozilla.fenix.library.bookmarks.SignInView

@SuppressWarnings("TooManyFunctions")
class SelectBookmarkFolderFragment : Fragment(), AccountObserver {

    private val sharedViewModel: BookmarksSharedViewModel by activityViewModels()
    private var folderGuid: String? = null
    private var bookmarkNode: BookmarkNode? = null
    private lateinit var signInView: SignInView
    private lateinit var bookmarkInteractor: SelectBookmarkFolderInteractor

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_select_bookmark_folder, container, false)

        bookmarkInteractor = SelectBookmarkFolderInteractor(
            context!!,
            findNavController(),
            sharedViewModel
        )
        signInView = SignInView(view.selectBookmarkLayout, bookmarkInteractor)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        sharedViewModel.signedIn.observe(this@SelectBookmarkFolderFragment, Observer<Boolean> {
            signInView.update(it)
        })
    }

    override fun onResume() {
        super.onResume()
        context?.let { setRootTitles(it, showMobileRoot = true) }
        activity?.title = getString(R.string.bookmark_select_folder_fragment_label)
        (activity as? AppCompatActivity)?.supportActionBar?.show()

        folderGuid = SelectBookmarkFolderFragmentArgs.fromBundle(arguments!!).folderGuid ?: BookmarkRoot.Root.id
        checkIfSignedIn()

        lifecycleScope.launch(Main) {
            bookmarkNode = withContext(IO) {
                requireComponents.core.bookmarksStorage.getTree(BookmarkRoot.Root.id, true)
                    .withOptionalDesktopFolders(context, showMobileRoot = true)
            }
            activity?.title = bookmarkNode?.title ?: getString(R.string.library_bookmarks)
            val adapter = SelectBookmarkFolderAdapter(sharedViewModel)
            recylerViewBookmarkFolders.adapter = adapter
            adapter.updateData(bookmarkNode)
        }
    }

    private fun checkIfSignedIn() {
        val accountManager = requireComponents.backgroundServices.accountManager
        accountManager.register(this, owner = this)
        accountManager.authenticatedAccount()?.let { bookmarkInteractor.onSignedIn() }
            ?: bookmarkInteractor.onSignedOut()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        val visitedAddBookmark = SelectBookmarkFolderFragmentArgs.fromBundle(arguments!!).visitedAddBookmark
        if (!visitedAddBookmark) {
            inflater.inflate(R.menu.bookmarks_select_folder, menu)
            menu.findItem(R.id.add_folder_button).icon.colorFilter =
                PorterDuffColorFilter(context!!.getColorFromAttr(R.attr.primaryText), SRC_IN)
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.add_folder_button -> {
                lifecycleScope.launch(Main) {
                    nav(
                        R.id.bookmarkSelectFolderFragment,
                        SelectBookmarkFolderFragmentDirections
                            .actionBookmarkSelectFolderFragmentToBookmarkAddFolderFragment()
                    )
                }
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
    override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
        bookmarkInteractor.onSignedIn()
    }

    override fun onLoggedOut() {
        bookmarkInteractor.onSignedOut()
    }
}
