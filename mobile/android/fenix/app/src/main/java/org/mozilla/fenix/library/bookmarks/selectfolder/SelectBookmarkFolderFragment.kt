/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.selectfolder

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentSelectBookmarkFolderBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel
import org.mozilla.fenix.library.bookmarks.DesktopFolders

class SelectBookmarkFolderFragment : Fragment() {
    private var _binding: FragmentSelectBookmarkFolderBinding? = null
    private val binding get() = _binding!!

    private val sharedViewModel: BookmarksSharedViewModel by activityViewModels()
    private var bookmarkNode: BookmarkNode? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        _binding = FragmentSelectBookmarkFolderBinding.inflate(inflater, container, false)

        return binding.root
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _binding = null
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.bookmark_select_folder_fragment_label))

        val args: SelectBookmarkFolderFragmentArgs by navArgs()

        viewLifecycleOwner.lifecycleScope.launch(Main) {
            bookmarkNode = withContext(IO) {
                val context = requireContext()
                context.components.core.bookmarksStorage
                    .getTree(BookmarkRoot.Root.id, recursive = true)
                    ?.let { DesktopFolders(context, showMobileRoot = true).withOptionalDesktopFolders(it) }
            }
            val adapter = SelectBookmarkFolderAdapter(sharedViewModel)
            binding.recylerViewBookmarkFolders.adapter = adapter
            adapter.updateData(bookmarkNode, args.hideFolderGuid)
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        val args: SelectBookmarkFolderFragmentArgs by navArgs()
        if (!args.allowCreatingNewFolder) {
            inflater.inflate(R.menu.bookmarks_select_folder, menu)
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.add_folder_button -> {
                viewLifecycleOwner.lifecycleScope.launch(Main) {
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
}
