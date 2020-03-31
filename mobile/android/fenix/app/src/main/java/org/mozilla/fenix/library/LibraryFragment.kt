/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_library.*
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setToolbarColors
import org.mozilla.fenix.ext.showToolbar

/**
 * Displays buttons to navigate to library sections, such as bookmarks and history.
 */
class LibraryFragment : Fragment(R.layout.fragment_library) {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onResume() {
        super.onResume()
        initToolbar()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        libraryHistory.setOnClickListener {
            requireComponents.analytics.metrics
                .track(Event.LibrarySelectedItem(Event.LibrarySelectedItem.LibraryItem.HISTORY))
            nav(
                R.id.libraryFragment,
                LibraryFragmentDirections.actionLibraryFragmentToHistoryFragment()
            )
        }

        libraryBookmarks.setOnClickListener {
            requireComponents.analytics.metrics
                .track(Event.LibrarySelectedItem(Event.LibrarySelectedItem.LibraryItem.BOOKMARKS))
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

    override fun onDestroy() {
        super.onDestroy()
        requireComponents.analytics.metrics.track(Event.LibraryClosed)
    }

    private fun initToolbar() {
        (activity as? AppCompatActivity)?.let { activity ->
            val toolbar = activity.findViewById<Toolbar>(R.id.navigationToolbar)
            toolbar?.setToolbarColors(
                foreground = activity.getColorFromAttr(R.attr.primaryText),
                background = activity.getColorFromAttr(R.attr.foundation)
            )
            showToolbar(getString(R.string.library_title))
        }
    }
}
