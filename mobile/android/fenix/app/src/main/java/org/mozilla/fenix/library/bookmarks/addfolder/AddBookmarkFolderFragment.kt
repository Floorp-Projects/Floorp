/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.addfolder

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.View.GONE
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_edit_bookmark.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel

/**
 * Menu to create a new bookmark folder.
 */
class AddBookmarkFolderFragment : Fragment(R.layout.fragment_edit_bookmark) {

    private val sharedViewModel: BookmarksSharedViewModel by activityViewModels {
        ViewModelProvider.NewInstanceFactory() // this is a workaround for #4652
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    /**
     * Hides fields for bookmark items present in the shared layout file.
     */
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        bookmarkUrlLabel.visibility = GONE
        bookmarkUrlEdit.visibility = GONE
        bookmarkNameEdit.showKeyboard()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.bookmark_add_folder_fragment_label))

        viewLifecycleOwner.lifecycleScope.launch(Main) {
            sharedViewModel.selectedFolder = withContext(IO) {
                sharedViewModel.selectedFolder
                    ?: requireComponents.core.bookmarksStorage.getTree(BookmarkRoot.Mobile.id)
            }

            bookmarkParentFolderSelector.text = sharedViewModel.selectedFolder!!.title
            bookmarkParentFolderSelector.setOnClickListener {
                nav(
                    R.id.bookmarkAddFolderFragment,
                    AddBookmarkFolderFragmentDirections
                        .actionBookmarkAddFolderFragmentToBookmarkSelectFolderFragment(
                            true
                        )
                )
            }
        }
    }

    override fun onPause() {
        super.onPause()
        bookmarkNameEdit.hideKeyboard()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.bookmarks_add_folder, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.confirm_add_folder_button -> {
                if (bookmarkNameEdit.text.isNullOrBlank()) {
                    bookmarkNameEdit.error =
                        getString(R.string.bookmark_empty_title_error)
                    return true
                }
                this.view?.hideKeyboard()
                viewLifecycleOwner.lifecycleScope.launch(IO) {
                    val newGuid = requireComponents.core.bookmarksStorage.addFolder(
                        sharedViewModel.selectedFolder!!.guid, bookmarkNameEdit.text.toString(), null
                    )
                    sharedViewModel.selectedFolder = requireComponents.core.bookmarksStorage.getTree(newGuid)
                    requireComponents.analytics.metrics.track(Event.AddBookmarkFolder)
                    withContext(Main) {
                        Navigation.findNavController(requireActivity(), R.id.container).popBackStack()
                    }
                }
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
}
