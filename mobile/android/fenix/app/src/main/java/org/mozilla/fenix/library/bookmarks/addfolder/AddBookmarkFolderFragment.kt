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
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.databinding.FragmentEditBookmarkBinding
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel
import org.mozilla.fenix.library.bookmarks.friendlyRootTitle

/**
 * Menu to create a new bookmark folder.
 */
class AddBookmarkFolderFragment : Fragment(R.layout.fragment_edit_bookmark) {
    private var _binding: FragmentEditBookmarkBinding? = null
    private val binding get() = _binding!!

    private val sharedViewModel: BookmarksSharedViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    /**
     * Hides fields for bookmark items present in the shared layout file.
     */
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        _binding = FragmentEditBookmarkBinding.bind(view)

        binding.bookmarkUrlLabel.visibility = GONE
        binding.bookmarkUrlEdit.visibility = GONE
        binding.inputLayoutBookmarkUrl.visibility = GONE
        binding.bookmarkNameEdit.showKeyboard()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.bookmark_add_folder_fragment_label))

        viewLifecycleOwner.lifecycleScope.launch(Main) {
            val context = requireContext()
            sharedViewModel.selectedFolder = withContext(IO) {
                sharedViewModel.selectedFolder
                    ?: requireComponents.core.bookmarksStorage.getBookmark(BookmarkRoot.Mobile.id)
            }

            binding.bookmarkParentFolderSelector.text =
                friendlyRootTitle(context, sharedViewModel.selectedFolder!!)
            binding.bookmarkParentFolderSelector.setOnClickListener {
                nav(
                    R.id.bookmarkAddFolderFragment,
                    AddBookmarkFolderFragmentDirections
                        .actionBookmarkAddFolderFragmentToBookmarkSelectFolderFragment(
                            allowCreatingNewFolder = true
                        )
                )
            }
        }
    }

    override fun onPause() {
        super.onPause()
        binding.bookmarkNameEdit.hideKeyboard()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.bookmarks_add_folder, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.confirm_add_folder_button -> {
                if (binding.bookmarkNameEdit.text.isNullOrBlank()) {
                    binding.bookmarkNameEdit.error =
                        getString(R.string.bookmark_empty_title_error)
                    return true
                }
                this.view?.hideKeyboard()
                viewLifecycleOwner.lifecycleScope.launch(IO) {
                    val newGuid = requireComponents.core.bookmarksStorage.addFolder(
                        sharedViewModel.selectedFolder!!.guid,
                        binding.bookmarkNameEdit.text.toString(),
                        null
                    )
                    sharedViewModel.selectedFolder =
                        requireComponents.core.bookmarksStorage.getTree(newGuid)
                    requireComponents.analytics.metrics.track(Event.AddBookmarkFolder)
                    withContext(Main) {
                        Navigation.findNavController(requireActivity(), R.id.container)
                            .popBackStack()
                    }
                }
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _binding = null
    }
}
