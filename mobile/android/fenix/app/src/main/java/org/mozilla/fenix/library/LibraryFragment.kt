/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

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
import androidx.appcompat.widget.Toolbar
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_library.*
import mozilla.appservices.places.BookmarkRoot
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents

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

        setToolbarColor()
        (activity as AppCompatActivity).title = getString(R.string.library_title)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        libraryHistory.setOnClickListener {
            requireComponents.analytics.metrics
                .track(Event.LibrarySelectedItem(view.context.getString(R.string.library_history)))
            nav(R.id.libraryFragment, LibraryFragmentDirections.actionLibraryFragmentToHistoryFragment())
        }

        libraryBookmarks.setOnClickListener {
            requireComponents.analytics.metrics
                .track(Event.LibrarySelectedItem(view.context.getString(R.string.library_bookmarks)))
            nav(
                R.id.libraryFragment,
                LibraryFragmentDirections.actionLibraryFragmentToBookmarksFragment(BookmarkRoot.Mobile.id)
            )
        }

        requireComponents.analytics.metrics.track(Event.LibraryOpened)
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
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        requireComponents.analytics.metrics.track(Event.LibraryClosed)
    }

    private fun setToolbarColor() {
        val toolbar = (activity as AppCompatActivity).findViewById<Toolbar>(R.id.navigationToolbar)

        val backgroundColor = R.attr.foundation.getColorFromAttr(context!!)
        val foregroundColor = R.attr.primaryText.getColorFromAttr(context!!)

        toolbar.setBackgroundColor(backgroundColor)
        toolbar.setTitleTextColor(foregroundColor)
        toolbar.navigationIcon?.colorFilter =
            PorterDuffColorFilter(foregroundColor, SRC_IN)
    }
}
