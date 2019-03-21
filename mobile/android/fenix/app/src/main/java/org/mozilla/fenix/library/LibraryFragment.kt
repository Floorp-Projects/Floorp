/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

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
import kotlinx.android.synthetic.main.fragment_library.*
import mozilla.appservices.places.BookmarkRoot
import org.mozilla.fenix.R
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentArgs
import org.mozilla.fenix.utils.ItsNotBrokenSnack

class LibraryFragment : Fragment() {

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_library, container, false)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        libraryHistory.setOnClickListener(
            Navigation.createNavigateOnClickListener(
                LibraryFragmentDirections.actionLibraryFragmentToHistoryFragment().actionId,
                null
            )
        )

        libraryBookmarks.setOnClickListener(Navigation.createNavigateOnClickListener(
            LibraryFragmentDirections.actionLibraryFragmentToBookmarksFragment(BookmarkRoot.Root.id).actionId,
            BookmarkFragmentArgs(BookmarkRoot.Root.id).toBundle()
        ))

        libraryDownloads.setOnClickListener {
            ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "348")
        }
        libraryScreenshots.setOnClickListener {
            ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "89")
        }
        libraryReadingList.setOnClickListener {
            ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "913")
        }
        librarySessions.setOnClickListener {
            ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "637")
        }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.library_menu, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.libraryClose -> {
                Navigation.findNavController(requireActivity(), R.id.container).navigateUp()
                true
            }
            R.id.librarySearch -> {
                // TODO Library Search
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1118")
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
}
